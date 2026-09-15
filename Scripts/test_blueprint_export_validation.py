"""Source-to-snapshot and ReadPack fidelity checks without retired Markdown views."""

import copy
import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("validate_blueprint_ai_export.py")
SPEC = importlib.util.spec_from_file_location("validator", SCRIPT)
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


class BlueprintExportValidationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.pack = Path(temporary.name)
        nodes = [
            {"id": "A", "name": "Start", "node_guid": "00000001000000000000000000000001",
             "class": "K2Node_Event", "class_path": "/Script/BlueprintGraph.K2Node_Event",
             "comment": "", "is_enabled": True, "semantic_status": "classified",
             "semantic": {"kind": "event", "event": {"name": "Start"}},
             "pins": [{"index": 0, "name": "then", "direction": "output", "is_exec": True}]},
            {"id": "B", "name": "Finish", "node_guid": "00000001000000000000000000000002",
             "class": "K2Node_CallFunction", "class_path": "/Script/BlueprintGraph.K2Node_CallFunction",
             "comment": "", "is_enabled": True, "semantic_status": "classified",
             "semantic": {"kind": "call_function", "function": {"name": "Finish"}},
             "pins": [{"index": 0, "name": "execute", "direction": "input", "is_exec": True}]},
        ]
        edge = {"kind": "exec", "from_node": {"node_id": "A"}, "from_pin_index": 0,
                "to_node": {"node_id": "B"}, "to_pin_index": 0}
        graph = {"path": "/Game/Test.Test:EventGraph", "name": "EventGraph", "nodes": nodes, "edges": [edge]}
        self.write_json("90_Full/Test.json", {"asset_path": "/Game/Test.Test", "graphs": [graph]})
        self.write_json("20_Evidence/00_Asset.json", {"asset_path": "/Game/Test.Test"})
        self.write_json("20_Evidence/G0001.json", graph)
        self.write("02_ReadingContract.md", "Fixture reading contract")
        self.write("05_Query.py", "# Fixture query resource\n")
        self.write("10_Logic/G0001.pseudo.md",
                   '```text\n@A: event "Start"()\n  exit "then" -> @B["execute"]\n'
                   '@B: call "Finish"()\n```\n')
        self.write("00_START_HERE.md", "02_ReadingContract.md " + self.sha("02_ReadingContract.md")
                   + " 90_Full/ 10_Logic/G0001.pseudo.md 20_Evidence/G0001.json")
        record = {"id": "G0001", "path": graph["path"], "node_count": len(nodes),
                  "logic": "10_Logic/G0001.pseudo.md", "evidence": "20_Evidence/G0001.json"}
        for kind in ("logic", "evidence"):
            record[kind + "_sha1"] = self.sha(record[kind])
            record[kind + "_bytes"] = (self.pack / record[kind]).stat().st_size
        self.manifest = {
            "format_version": 2, "pseudo_format_version": 1, "asset_path": "/Game/Test.Test",
            "features": ["format_contract"], "graphs": [record],
            "query": {"protocol_version": 1, "path": "05_Query.py", "sha1": self.sha("05_Query.py")},
            "reading_contract": {"version": 1, "path": "02_ReadingContract.md",
                                 "sha1": self.sha("02_ReadingContract.md")},
        }
        self.write_json("01_Manifest.json", self.manifest)
        source = copy.deepcopy(graph)
        source["edges"] = [{"kind": "exec", "from_name": "Start", "from_index": 0,
                            "to_name": "Finish", "to_index": 0}]
        self.asset = {"requested_path": "/Game/Test.Test", "export_directory": str(self.pack),
                      "export_succeeded": True, "dirty_before": False, "dirty_after": False,
                      "source_graphs": [source], "source_graphs_after": [copy.deepcopy(source)]}

    def write(self, relative, text):
        path = self.pack / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")

    def write_json(self, relative, value):
        self.write(relative, json.dumps(value, ensure_ascii=False))

    def sha(self, relative):
        return hashlib.sha1((self.pack / relative).read_bytes()).hexdigest()

    def assert_rejected(self, expected_error):
        result = validator.validate(self.asset)
        self.assertFalse(result["passed"])
        self.assertTrue(any(expected_error in error for error in result["errors"]), result["errors"])

    def test_without_legacy_markdown_passes(self):
        result = validator.validate(self.asset)
        self.assertTrue(result["passed"], result["errors"])
        self.assertNotIn("ai_path", result)

    def test_source_node_mismatch(self):
        for field in ("source_graphs", "source_graphs_after"):
            self.asset[field][0]["nodes"][0]["comment"] = "changed"
        self.assert_rejected("Source node")

    def test_source_pin_mismatch(self):
        for field in ("source_graphs", "source_graphs_after"):
            self.asset[field][0]["nodes"][0]["pins"][0]["name"] = "wrong"
        self.assert_rejected("Source pin")

    def test_source_edge_mismatch(self):
        for field in ("source_graphs", "source_graphs_after"):
            self.asset[field][0]["edges"][0]["to_index"] = 9
        self.assert_rejected("Exact edge multiset")

    def test_asset_evidence_mismatch(self):
        self.write_json("20_Evidence/00_Asset.json", {"asset_path": "/Game/Wrong"})
        self.assert_rejected("Read pack asset evidence differs")

    def test_graph_evidence_mismatch(self):
        graph = validator.load(self.pack / "20_Evidence/G0001.json")
        graph["nodes"][0]["pins"][0]["name"] = "wrong"
        self.write_json("20_Evidence/G0001.json", graph)
        self.assert_rejected("Read pack full graph evidence differs")

    def test_pseudo_semantic_change_with_fresh_hash(self):
        path = "10_Logic/G0001.pseudo.md"
        self.write(path, (self.pack / path).read_text(encoding="utf-8").replace('"Finish"', '"Wrong"'))
        self.manifest["graphs"][0].update(logic_sha1=self.sha(path), logic_bytes=(self.pack / path).stat().st_size)
        self.write_json("01_Manifest.json", self.manifest)
        self.assert_rejected("Pseudo")

    def test_hash_mismatch(self):
        self.manifest["graphs"][0]["logic_sha1"] = "0" * 40
        self.write_json("01_Manifest.json", self.manifest)
        self.assert_rejected("logic_sha1 differs")

    def test_export_mutated_source(self):
        self.asset["source_graphs_after"][0]["nodes"][0]["comment"] = "changed"
        self.assert_rejected("Export mutated loaded source graph")

    def test_export_dirtied_asset(self):
        self.asset["dirty_after"] = True
        self.assert_rejected("Export changed source package dirty state")


if __name__ == "__main__":
    unittest.main()
