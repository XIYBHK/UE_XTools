"""Offline query regressions; no Unreal installation or network required."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).parents[1] / "Resources/BlueprintExport/05_Query.py"


class QueryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name) / "Caller"
        self.root.mkdir()
        self.addCleanup(self.tmp.cleanup)
        (self.root / "10_Logic").mkdir()
        (self.root / "20_Evidence").mkdir()
        self.records = []
        self.make_graph("G0001", "Event", ["Other", "Prior", "Pure", "Tail", "N0", "N1", "N2"])
        self.make_graph("G0002", "Other", ["N0"])

    def write_manifest(self):
        (self.root / "01_Manifest.json").write_text(json.dumps({
            "format_version": 2, "asset_path": "/Game/Test.Test", "snapshot_id": "fixture",
            "graphs": self.records}), encoding="utf-8")

    def make_graph(self, gid, name, ids):
        logic = "# Graph\n```text\n" + "\n\n".join(
            f'@{i}: opaque "{i}"("zero"=0, "flag"=false, "empty"=serialized(""))'
            for i in ids) + "\n```\nThis footer must not be returned.\n"
        nodes = [{"id": i, "name": i, "title": "Title " + i, "semantic": {"kind": "call"},
                  "pins": [{"is_exec": i != "Pure"}]} for i in ids]
        edges = []
        if gid == "G0001":
            for kind, source, target in (("exec", "N0", "N1"), ("exec", "N1", "N2"),
                                         ("exec", "N2", "N1"), ("exec", "Other", "N2"),
                                         ("data", "Prior", "N1"), ("data", "Pure", "Prior"),
                                         ("exec", "Prior", "Tail")):
                edges.append({"kind": kind, "from_node": {"node_id": source}, "from_pin_index": 1,
                              "to_node": {"node_id": target}, "to_pin_index": 0})
        evidence = {"path": "/Game/Test.Test:" + name, "nodes": nodes, "edges": edges}
        lp, ep = f"10_Logic/{gid}.pseudo.md", f"20_Evidence/{gid}.json"
        (self.root / lp).write_text(logic, encoding="utf-8")
        (self.root / ep).write_text(json.dumps(evidence), encoding="utf-8")
        self.records.append({"id": gid, "name": name, "path": evidence["path"], "logic": lp,
                             "evidence": ep, "node_count": len(nodes),
                             "entries": [{"id": "N0", "kind": "event", "name": "Start"}]})
        self.rehash()

    def rehash(self):
        for record in self.records:
            for key in ("logic", "evidence"):
                record[key + "_sha1"] = hashlib.sha1((self.root / record[key]).read_bytes()).hexdigest()
        self.write_manifest()

    def invoke(self, *args, success=True):
        process = subprocess.run([sys.executable, "-X", "utf8", str(TOOL), *args,
                                  "--directory", str(self.root)], text=True, encoding="utf-8", capture_output=True)
        self.assertEqual(process.returncode == 0, success, process.stdout + process.stderr)
        self.last_output = process.stdout
        return json.loads(process.stdout)

    def test_execution_loop_shared_target_and_prior_dependency(self):
        result = self.invoke("slice", "--graph", "G0001", "--node", "N0")
        self.assertEqual([n["id"] for n in result["nodes"]], ["N0", "N1", "N2", "Prior", "Pure"])
        self.assertTrue(result["nodes"][3]["requires_prior_execution"])
        self.assertNotIn("requires_prior_execution", result["nodes"][4])
        self.assertEqual(len(result["edges"]), 5)
        self.assertEqual(result["boundary_edge_count"], 2)
        self.assertFalse(result["truncated"])
        self.assertNotIn('"pins"', self.last_output)
        self.assertIn('"flag"=false', result["nodes"][0]["logic"])

    def test_budget_preserves_seed_and_recomputes_edges(self):
        result = self.invoke("slice", "--graph", "G0001", "--node", "N0", "--max-nodes", "1")
        self.assertEqual([n["id"] for n in result["nodes"]], ["N0"])
        self.assertEqual(result["edges"], [])
        self.assertEqual(result["boundary_edge_count"], 1)
        self.assertEqual(result["remaining_nodes"], 4)
        self.assertTrue(result["truncated"])
        for budget in (600, 800, 1200, 2000):
            with self.subTest(budget=budget):
                result = self.invoke("slice", "--graph", "G0001", "--node", "N0", "--max-chars", str(budget))
                self.assertLessEqual(len(self.last_output), budget)
                kept = {n["id"] for n in result["nodes"]}
                self.assertTrue(all(e["from"][0] in kept and e["to"][0] in kept for e in result["edges"]))

    def test_outline_and_find_are_budgeted_without_merging_graph_ids(self):
        self.assertEqual(len(self.invoke("find", "--query", "Title N0")["results"]), 2)
        limited = self.invoke("find", "--query", "Title", "--max-nodes", "1")
        self.assertEqual(len(limited["results"]), 1)
        self.assertTrue(limited["truncated"])
        self.assertTrue(self.invoke("outline", "--max-nodes", "1")["truncated"])
        self.invoke("outline", "--max-chars", "400")
        self.assertLessEqual(len(self.last_output), 400)
        (self.root / "20_Evidence/G0001.json").write_text("corrupt", encoding="utf-8")
        self.invoke("outline")
        self.invoke("node", "--graph", "G0002", "--node", "N0")
        self.invoke("node", "--graph", "G0001", "--node", "N0", success=False)

    def test_node_evidence_and_required_selectors(self):
        result = self.invoke("node", "--graph", "Event", "--node", "@N0", "--evidence")
        self.assertIn("pins", result["results"][0]["node"])
        result = self.invoke("node", "--graph", "G0002", "--node", "N0")
        self.assertNotIn("footer", result["results"][0]["logic"])
        self.invoke("node", "--node", "N0", success=False)
        self.invoke("slice", "--graph", "G0001", success=False)
        self.invoke("find", success=False)
        self.invoke("node", "--graph", "G0001", "--node", "missing", success=False)
        self.records[1]["name"] = "Event"
        self.write_manifest()
        self.invoke("node", "--graph", "Event", "--node", "N0", success=False)

    def test_hash_path_and_identity_rejection(self):
        record = self.records[0]
        original = record["logic"]
        record["logic"] = "../escape.pseudo.md"
        self.write_manifest()
        self.assertIn("escapes", self.invoke("node", "--graph", "G0001", "--node", "N0", success=False)["error"])
        record["logic"] = original
        del record["logic_sha1"]
        self.write_manifest()
        self.assertIn("missing", self.invoke("node", "--graph", "G0001", "--node", "N0", success=False)["error"])
        ep = self.root / record["evidence"]
        evidence = json.loads(ep.read_text())
        for path in (None, "/Game/Wrong"):
            evidence["path"] = path
            ep.write_text(json.dumps(evidence), encoding="utf-8")
            self.rehash()
            self.assertIn("path mismatch", self.invoke("node", "--graph", "G0001", "--node", "N0", success=False)["error"])

    def test_disabled_comment_and_duplicate_block(self):
        path = self.root / self.records[1]["logic"]
        path.write_text('```text\ndisabled development_only @N0: opaque "N0"()\n```\n', encoding="utf-8")
        self.rehash()
        self.assertIn("disabled development_only", self.invoke("node", "--graph", "G0002", "--node", "N0")["results"][0]["logic"])
        path.write_text('```text\nauthor_comment @N0 = "comment"\n```\n', encoding="utf-8")
        self.rehash()
        self.assertIn("author_comment", self.invoke("node", "--graph", "G0002", "--node", "N0")["results"][0]["logic"])
        path.write_text('```text\n@N0: opaque "N0"()\n@N0: opaque "N0"()\n```\n', encoding="utf-8")
        self.rehash()
        self.assertIn("duplicate", self.invoke("node", "--graph", "G0002", "--node", "N0", success=False)["error"])

    def test_deps_requires_graph_and_keeps_native_or_unresolved_explicit(self):
        evidence_path = self.root / self.records[0]["evidence"]
        evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
        evidence["nodes"][0]["semantic"] = {
            "kind": "call_function", "resolved_function": "/Script/Engine.Actor:K2_DestroyActor"}
        evidence["nodes"][1]["semantic"] = {
            "kind": "call_function", "resolved_function": "ShortName:Func"}
        evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
        self.rehash()
        result = self.invoke("deps", "--graph", "G0001")
        self.assertEqual(result["results"][0]["status"], "native_implementation")
        self.assertEqual(result["results"][1]["status"], "unresolved")
        self.invoke("deps", success=False)

    def test_node_dependency_hint_does_not_scan(self):
        evidence_path = self.root / self.records[0]["evidence"]
        evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
        next(node for node in evidence["nodes"] if node["id"] == "N0")["semantic"] = {
            "kind": "macro_instance", "macro_graph": "/Game/Other.Other:Macro"}
        evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
        self.rehash()
        result = self.invoke("node", "--graph", "G0001", "--node", "N0")
        self.assertIn("dependency_hint", result["results"][0])
        self.assertNotIn("results", self.invoke("deps", "--graph", "G0001", "--node", "N0")["results"][0])

    def set_dependency(self, raw, kind="call_function"):
        path = self.root / self.records[0]["evidence"]
        data = json.loads(path.read_text(encoding="utf-8"))
        next(n for n in data["nodes"] if n["id"] == "N0")["semantic"] = {
            "kind": kind, "resolved_function" if kind == "call_function" else "macro_graph": raw}
        path.write_text(json.dumps(data), encoding="utf-8")
        self.rehash()

    def target_export(self, dirname=None, asset="/Game/库/Target.Target"):
        name = dirname or asset.rsplit(".", 1)[1] + "_" + hashlib.sha1(asset.encode()).hexdigest()
        root = self.root.parent / name
        root.mkdir()
        (root / "00_START_HERE.md").write_text("entry", encoding="utf-8")
        (root / "logic.md").write_text('```text\n@N0: opaque "body"()\n```\n', encoding="utf-8")
        (root / "evidence.json").write_text(json.dumps({"path": asset + ":Sum",
            "nodes": [{"id": "N0"}], "edges": []}), encoding="utf-8")
        record = {"id": "G0045", "name": "Sum", "path": asset + ":Sum", "node_count": 1,
                  "entries": [], "logic": "logic.md", "evidence": "evidence.json"}
        for key in ("logic", "evidence"):
            record[key + "_sha1"] = hashlib.sha1((root / record[key]).read_bytes()).hexdigest()
        (root / "01_Manifest.json").write_text(json.dumps({"format_version": 2,
            "asset_path": asset, "snapshot_id": "target", "graphs": [record]}), encoding="utf-8")
        return root

    def deps(self, *args):
        return self.invoke("deps", "--graph", "G0001", "--node", "N0", *args)

    def test_late_export_skeleton_normal_and_macro_navigation(self):
        self.set_dependency("/Game/库/Target.SKEL_Target_C:Sum")
        self.assertEqual(self.deps()["results"][0]["status"], "not_exported")
        target = self.target_export()
        for raw, kind in (("/Game/库/Target.SKEL_Target_C:Sum", "call_function"),
                          ("/Game/库/Target.Target_C:Sum", "call_function"),
                          ("/Game/库/Target.Target:Sum", "macro_instance")):
            self.set_dependency(raw, kind)
            result = self.deps()["results"][0]
            self.assertEqual(result["status"], "ok")
            self.assertEqual(result["graph"], "G0045")
            self.assertEqual(result["asset_path"], "/Game/库/Target.Target")
            self.assertEqual((self.root / result["entry"]).resolve(), target / "00_START_HERE.md")
            self.assertEqual((self.root / result["logic"]).resolve(), target / "logic.md")
            self.assertNotIn("body", self.last_output)
        bounded = self.deps("--max-chars", "400")
        self.assertLessEqual(len(self.last_output), 400)
        self.assertTrue(bounded["truncated"])

    def test_canonical_priority_and_ambiguous_legacy_copies(self):
        self.set_dependency("/Game/库/Target.Target_C:Sum")
        self.target_export("LegacyA")
        self.target_export("LegacyB")
        self.assertEqual(self.deps()["results"][0]["status"], "ambiguous")
        canonical = self.target_export()
        self.assertEqual(self.deps()["results"][0]["status"], "ok")
        (canonical / "logic.md").write_text("stale", encoding="utf-8")
        self.assertEqual(self.deps()["results"][0]["status"], "invalid_export")
        (canonical / "01_Manifest.json").write_text("broken", encoding="utf-8")
        self.assertEqual(self.deps()["results"][0]["status"], "invalid_export")

    def test_exact_identity_missing_target_and_same_asset_macro(self):
        self.target_export(asset="/Game/Elsewhere/Target.Target")
        self.set_dependency("/Game/库/Target.Target_C:Sum")
        self.assertEqual(self.deps()["results"][0]["status"], "not_exported")
        for raw in ("", "Sum", "/Game/库/Target.Other_C:Sum", "/Game/库/Target.REINST_Target_C:Sum"):
            self.set_dependency(raw)
            self.assertEqual(self.deps()["results"][0]["status"], "unresolved")
        (self.root / "00_START_HERE.md").write_text("entry", encoding="utf-8")
        self.set_dependency("/Game/Test.Test:Other", "macro_instance")
        self.assertEqual(self.deps()["results"][0]["graph"], "G0002")

    def test_target_missing_graph_file_and_path_escape(self):
        self.set_dependency("/Game/库/Target.Target_C:Missing")
        target = self.target_export()
        self.assertEqual(self.deps()["results"][0]["status"], "graph_not_exported")
        self.set_dependency("/Game/库/Target.Target_C:Sum")
        (target / "logic.md").unlink()
        self.assertEqual(self.deps()["results"][0]["status"], "invalid_export")
        path = target / "01_Manifest.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["graphs"][0]["logic"] = "../Caller/10_Logic/G0001.pseudo.md"
        path.write_text(json.dumps(data), encoding="utf-8")
        self.assertEqual(self.deps()["results"][0]["status"], "invalid_export")


if __name__ == "__main__":
    unittest.main()
