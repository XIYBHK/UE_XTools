"""Offline evidence snapshot regressions; default loads the shipped query module."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest


TOOL = Path(__file__).parents[1] / "Resources/BlueprintExport/05_Query.py"


def guid(number):
    return f"{number:032X}"


def node(number):
    return {"id": f"N{number}", "node_guid": guid(number + 1), "title": "Node " + str(number),
            "name": "K2Node_" + str(number), "pos_x": number * 20, "pos_y": 0,
            "semantic": {"kind": "test", "id": "N0"},
            "pins": [{"id": guid(100 + number), "index": 0, "name": "Value", "default": "0",
                      "direction": "input", "type": {"category": "int"}, "linked_to": []}]}


class SnapshotDiffTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location("blueprint_snapshot_query", TOOL)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.old = Path(self.temp.name) / "old"
        self.new = Path(self.temp.name) / "new"
        self.args = SimpleNamespace(max_nodes=100, max_chars=100000)
        self.graph = {"path": "/Game/Test.Test:EventGraph", "name": "EventGraph", "graph_guid": guid(999),
                      "type": "event", "truncated": False, "nodes": [node(0), node(1)], "edges": []}
        self.connect(self.graph, 0, 1)

    @staticmethod
    def connect(graph, source, target):
        a, b = graph["nodes"][source], graph["nodes"][target]
        graph["edges"] = [{"kind": "data", "from_node": {"node_id": a["id"], "node_guid": a["node_guid"]},
                           "from_pin_id": a["pins"][0]["id"], "from_pin_index": 0,
                           "to_node": {"node_id": b["id"], "node_guid": b["node_guid"]},
                           "to_pin_id": b["pins"][0]["id"], "to_pin_index": 0}]
        for item in graph["nodes"]:
            item["pins"][0]["linked_to"] = []
        a["pins"][0]["linked_to"] = [{"node_id": b["id"], "node_guid": b["node_guid"],
                                      "pin_id": b["pins"][0]["id"], "pin_index": 0}]

    @staticmethod
    def renumber(graph):
        mapping = {item["id"]: f"N{len(graph['nodes']) - index + 40}" for index, item in enumerate(graph["nodes"])}
        def update(value):
            if isinstance(value, dict):
                if "node_id" in value:
                    value["node_id"] = mapping[value["node_id"]]
                for item in value.values():
                    update(item)
            elif isinstance(value, list):
                for item in value:
                    update(item)
        update(graph)
        for item in graph["nodes"]:
            item["id"] = mapping[item["id"]]
        graph["nodes"].reverse()
        graph["edges"].reverse()

    def write(self, root, graphs, asset="/Game/Test.Test"):
        root.mkdir(exist_ok=True)
        records = []
        for index, graph in enumerate(graphs):
            gid = f"G{index:04d}"
            logic = "```text\n" + "\n".join(f'@{item["id"]}: classified "test"' for item in graph["nodes"]) + "\n```\n"
            record = {"id": gid, "name": graph["name"], "path": graph["path"], "node_count": len(graph["nodes"])}
            for key, body in (("logic", logic), ("evidence", json.dumps(graph))):
                record[key] = gid + (".md" if key == "logic" else ".json")
                content = body.encode("utf-8")
                (root / record[key]).write_bytes(content)
                record[key + "_sha1"] = hashlib.sha1(content).hexdigest()
            records.append(record)
        (root / "01_Manifest.json").write_text(json.dumps({"format_version": 2, "asset_path": asset,
            "snapshot_id": root.name, "graphs": records}), encoding="utf-8")

    def compare(self, new_graph=None, old_graph=None):
        self.write(self.old, [old_graph or self.graph])
        self.write(self.new, [new_graph or copy.deepcopy(self.graph)])
        return self.run_diff()

    def run_diff(self):
        return self.module.snapshot_diff(self.new, self.old, self.args, {"asset_path": "/Game/Test.Test"})

    def test_same_snapshot_has_no_changes(self):
        result = self.compare()
        self.assertEqual(result["results"], [])
        self.assertTrue(result["complete_comparison"])
        self.assertEqual(result["summary"]["matched_nodes"], 2)

    def test_layout_renumbering_does_not_change_logic_or_links(self):
        changed = copy.deepcopy(self.graph)
        changed["nodes"][0]["pos_y"] += 100
        self.renumber(changed)
        result = self.compare(changed)
        self.assertEqual(len(result["results"]), 1)
        self.assertEqual(result["results"][0]["categories"], ["layout"])
        self.assertEqual(result["summary"]["layout_only_nodes"], 1)

    def test_renumbering_only_has_no_changes(self):
        changed = copy.deepcopy(self.graph)
        self.renumber(changed)
        self.assertEqual(self.compare(changed)["results"], [])

    def test_default_and_semantic_changes_are_separate(self):
        changed = copy.deepcopy(self.graph)
        changed["nodes"][0]["pins"][0]["default"] = "5"
        changed["nodes"][0]["semantic"]["id"] = "N1"
        categories = self.compare(changed)["results"][0]["categories"]
        self.assertEqual(set(categories), {"defaults", "logic"})

    def test_pin_identity_changes_are_preserved(self):
        changed = copy.deepcopy(self.graph)
        changed["nodes"][1]["pins"][0]["id"] = guid(777)
        self.connect(changed, 0, 1)
        result = self.compare(changed)
        self.assertTrue(any("pin_identity" in item.get("categories", []) for item in result["results"]))
        self.assertTrue(any(item["change"] == "edges_changed" for item in result["results"]))

    def test_pin_type_change(self):
        changed = copy.deepcopy(self.graph)
        changed["nodes"][1]["pins"][0]["type"]["category"] = "float"
        self.assertEqual(self.compare(changed)["results"][0]["categories"], ["pins"])

    def test_rewire_cycle_is_detected(self):
        changed = copy.deepcopy(self.graph)
        self.connect(changed, 1, 0)
        result = self.compare(changed)
        self.assertTrue(any(item["change"] == "edges_changed" for item in result["results"]))
        self.assertEqual(result["summary"]["changed_nodes"], 2)

    def test_added_removed_nodes(self):
        changed = copy.deepcopy(self.graph)
        changed["nodes"][1] = node(2)
        self.connect(changed, 0, 1)
        result = self.compare(changed)
        self.assertEqual(result["summary"]["added_nodes"], 1)
        self.assertEqual(result["summary"]["removed_nodes"], 1)

    def test_graph_rename_matches_guid(self):
        changed = copy.deepcopy(self.graph)
        changed["path"], changed["name"] = "/Game/Test.Test:Renamed", "Renamed"
        result = self.compare(changed)
        self.assertEqual([item["change"] for item in result["results"]], ["graph_renamed"])
        self.assertEqual(result["results"][0]["matched_by"], "graph_guid")

    def test_graph_without_guid_falls_back_to_path(self):
        old, changed = copy.deepcopy(self.graph), copy.deepcopy(self.graph)
        old.pop("graph_guid")
        changed.pop("graph_guid")
        changed["nodes"][0]["pos_x"] += 1
        self.assertEqual(self.compare(changed, old)["results"][0]["matched_by"], "graph_path")

    def test_added_removed_graphs(self):
        changed = copy.deepcopy(self.graph)
        changed["path"], changed["name"], changed["graph_guid"] = "/Game/Test.Test:Other", "Other", guid(555)
        result = self.compare(changed)
        self.assertEqual({item["change"] for item in result["results"]}, {"graph_added", "graph_removed"})

    def test_bad_node_identities_are_not_guessed(self):
        for value in (None, "", "0" * 32, guid(1)):
            with self.subTest(guid=value):
                changed = copy.deepcopy(self.graph)
                changed["nodes"][1]["node_guid"] = value
                result = self.compare(changed)
                self.assertFalse(result["complete_comparison"])
                self.assertEqual(result["results"][0]["change"], "not_comparable")
                self.assertGreater(result["summary"]["invalid_identity_nodes"], 0)

    def test_truncated_graph_not_reported_as_full_diff(self):
        changed = copy.deepcopy(self.graph)
        changed["truncated"] = True
        result = self.compare(changed)
        self.assertFalse(result["complete_comparison"])
        self.assertEqual(result["results"][0]["reason"], "truncated_graph")

    def test_invalid_reference_not_silently_mapped(self):
        changed = copy.deepcopy(self.graph)
        changed["edges"][0]["to_node"]["node_guid"] = guid(888)
        result = self.compare(changed)
        self.assertFalse(result["complete_comparison"])
        self.assertEqual(result["results"][0]["reason"], "unresolved_node_reference")

    def test_output_budget_retains_summary(self):
        changed = copy.deepcopy(self.graph)
        for item in changed["nodes"]:
            item["pos_y"] += 3
        self.args.max_nodes = 1
        result = self.compare(changed)
        self.assertTrue(result["truncated"])
        self.assertEqual(result["remaining_results"], 1)
        self.assertEqual(result["summary"]["changed_nodes"], 2)

    def test_character_budget_counts_full_envelope(self):
        changed = copy.deepcopy(self.graph)
        changed["nodes"][0]["pos_y"] += 100
        self.args.max_chars = 1100
        result = self.compare(changed)
        self.assertLessEqual(len(self.module.encode(result)), 1100)

    def test_corrupt_evidence_rejected(self):
        self.compare()
        with (self.old / "G0000.json").open("ab") as stream:
            stream.write(b" ")
        with self.assertRaisesRegex(ValueError, "stale hash"):
            self.run_diff()

    def test_other_asset_rejected(self):
        self.compare()
        self.write(self.new, [self.graph], asset="/Game/Other.Other")
        with self.assertRaisesRegex(ValueError, "identical asset_path"):
            self.run_diff()

    def test_duplicate_graph_guid_uses_paths_not_arbitrary_pairing(self):
        first, second = copy.deepcopy(self.graph), copy.deepcopy(self.graph)
        second["name"], second["path"] = "Other", "/Game/Test.Test:Other"
        changed = copy.deepcopy(first)
        changed["nodes"][0]["pos_x"] += 1
        self.write(self.old, [first, second])
        self.write(self.new, [second, changed])
        result = self.run_diff()
        self.assertEqual(len(result["results"]), 1)
        self.assertEqual(result["results"][0]["matched_by"], "graph_path")
        self.assertEqual(result["summary"]["matched_graphs"], 2)

    def test_exec_link_order_is_not_discarded(self):
        old = copy.deepcopy(self.graph)
        old["nodes"].append(node(2))
        old["nodes"][0]["pins"][0]["linked_to"].append({"node_id": "N2", "node_guid": guid(3),
                                                       "pin_id": guid(102), "pin_index": 0})
        changed = copy.deepcopy(old)
        changed["nodes"][0]["pins"][0]["linked_to"].reverse()
        self.assertEqual(self.compare(changed, old)["results"][0]["categories"], ["connections"])

    def test_edge_array_order_is_ignored(self):
        old = copy.deepcopy(self.graph)
        back = copy.deepcopy(old["edges"][0])
        back["from_node"], back["to_node"] = back["to_node"], back["from_node"]
        back["from_pin_id"], back["to_pin_id"] = back["to_pin_id"], back["from_pin_id"]
        old["edges"].append(back)
        changed = copy.deepcopy(old)
        changed["edges"].reverse()
        self.assertEqual(self.compare(changed, old)["results"], [])


if __name__ == "__main__":
    unittest.main()
