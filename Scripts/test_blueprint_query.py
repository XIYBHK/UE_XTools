"""Offline query regressions; no Unreal installation or network required."""
import hashlib
import importlib.util
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
            "graphs": self.records, "macro_definitions": getattr(self, "macros", [])}), encoding="utf-8")

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
        for record in self.records + getattr(self, "macros", []):
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

    def test_assets_filters_pin_defaults_and_reports_scope(self):
        evidence_path = self.root / self.records[0]["evidence"]
        evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
        pins = [
            {"name": "Hard", "direction": "input", "type": {"category": "object"},
             "default_object": "/Game/Foo.Bar"},
            {"name": "Soft", "direction": "input", "type": {"category": "softobject"},
             "default": "/MyPlugin/Assets.Icon"},
            {"name": "Class", "direction": "input", "type": {"category": "softclass"},
             "default": "Texture2D'/Engine/BasicShapes.Cube_C'"},
            {"name": "Connected", "direction": "input", "type": {"category": "object"},
             "default_object": "/Game/Nope.Asset", "connected": True},
            {"name": "String", "direction": "input", "type": {"category": "string"},
             "default": "/Game/Fake.Asset"},
            {"name": "Bad", "direction": "input", "type": {"category": "object"},
             "default_object": "not-a-path"},
        ]
        evidence["nodes"][0]["pins"] = pins
        evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
        self.rehash()
        result = self.invoke("assets")
        self.assertEqual([item["pin"] for item in result["results"]], ["Hard", "Soft", "Class"])
        self.assertEqual(result["metadata"]["scope"], "typed_unconnected_input_defaults")
        self.assertFalse(result["metadata"]["complete_asset_graph"])
        self.assertEqual(result["metadata"]["unresolved"], 1)
        self.assertEqual(result["results"][1]["reference_kind"], "soft_object")
        self.invoke("assets", "--node", "N0", success=False)
        empty = self.invoke("assets", "--graph", "G0002")
        self.assertEqual(empty["results"], [])

    def test_assets_real_schema_guards_paths_and_budgets(self):
        def pin(value="/Game/特效/爆炸.爆炸", **overrides):
            return {"direction": "input", "name": "Asset", "default": value,
                    "type": {"category": "softobject", "container": "none"}, **overrides}
        valid = [pin(), pin("Texture2D'/Game/T.T'"),
                 pin("/Script/Engine.Texture2D'/Game/T.T'"),
                 pin("SoftObjectPath'/Plugin/Asset.Asset'"),
                 pin(default_object="/Game/BP.BP_C", type={"category": "class"})]
        ignored = [pin(connected=True), pin(linked_to=[{}]), pin(orphaned=True),
                   pin(default_value_ignored=True), pin(direction="output"), pin(is_exec=True),
                   pin(""), pin("None"), pin(direction="")]
        unsupported = [pin(type={"category": "object", "container": "array"}),
                       pin(type={"category": "string"})]
        invalid = [pin(value) for value in ("/Game/Bad Path.A", "/Game/A.A'",
                   "/Game/A.A\n", "/Script/Engine.Actor", "/Temp/A.A", "/Memory/A.A",
                   "/Transient/A.A", "/Game/../A.A", "/Game/A.A;evil", "garbage'/Game/A.A")]
        evidence_path = self.root / self.records[0]["evidence"]
        evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
        evidence["nodes"][0]["pins"] = valid + ignored + unsupported + invalid
        evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
        self.rehash()
        result = self.invoke("assets", "--graph", "G0001", "--node", "Other")
        self.assertEqual(len(result["results"]), len(valid))
        self.assertEqual(result["metadata"]["unresolved"], len(invalid))
        self.assertEqual(result["metadata"]["unsupported"], len(unsupported))
        self.assertTrue(all(item["status"] == "reference_only" for item in result["results"]))
        self.assertEqual(result["results"][-1]["reference_kind"], "hard_class")
        limited = self.invoke("assets", "--graph", "G0001", "--max-nodes", "1")
        self.assertTrue(limited["truncated"])
        self.assertEqual(limited["remaining_results"], len(valid) - 1)
        self.invoke("assets", "--max-chars", "600")
        self.assertLessEqual(len(self.last_output), 600)
        self.invoke("assets", "--graph", "G0001", "--node", "missing", success=False)
        deps = self.invoke("deps", "--graph", "G0001")
        self.assertEqual(deps["metadata"]["scope"], "forward_function_and_macro_calls")
        self.assertIn("assets", deps["metadata"]["asset_reference_hint"])

    def test_assets_macro_body_requires_explicit_selection(self):
        self.make_graph("M0001", "Macro", ["N0"])
        self.macros = [self.records.pop()]
        path = self.root / self.macros[0]["evidence"]
        evidence = json.loads(path.read_text(encoding="utf-8"))
        evidence["nodes"][0]["pins"] = [{"direction": "input", "name": "Asset",
            "type": {"category": "object"}, "default_object": "/Engine/T.T"}]
        path.write_text(json.dumps(evidence), encoding="utf-8")
        self.rehash()
        self.assertEqual(self.invoke("assets")["results"], [])
        self.assertEqual(self.invoke("assets", "--graph", "M0001")["results"][0]["path"], "/Engine/T.T")

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

    def make_macro_definition(self):
        (self.root / "30_Dependencies").mkdir(exist_ok=True)
        lp, ep = "30_Dependencies/M0001.pseudo.md", "30_Dependencies/M0001.json"
        logic = '```text\n@N0: opaque "N0"()\n```\n'
        evidence = {"path": "/Engine/BasicShapes/BasicShapes.BasicShapes:EngineMacro",
                    "nodes": [{"id": "N0"}], "edges": []}
        (self.root / lp).write_text(logic, encoding="utf-8")
        (self.root / ep).write_text(json.dumps(evidence), encoding="utf-8")
        (self.root / "00_START_HERE.md").write_text("entry", encoding="utf-8")
        self.macros = [{"id": "M0001", "name": "EngineMacro",
                        "path": evidence["path"], "logic": lp, "evidence": ep,
                        "node_count": 1, "entries": []}]
        self.rehash()

    def test_macro_definitions_are_selectable_only_explicitly_and_deps_stay_local(self):
        self.make_macro_definition()
        outline = self.invoke("outline")
        self.assertNotIn("M0001", {item.get("graph") for item in outline["results"]})
        node = self.invoke("node", "--graph", "M0001", "--node", "N0")
        self.assertEqual(node["results"][0]["graph"], "M0001")
        self.set_dependency("/Engine/BasicShapes/BasicShapes.BasicShapes:EngineMacro", "macro_instance")
        result = self.deps()["results"][0]
        self.assertEqual(result["status"], "ok")
        self.assertEqual(result["export_owner_asset_path"], "/Game/Test.Test")
        self.assertEqual(result["query_directory"], ".")
        self.assertEqual(result["asset_path"], "/Engine/BasicShapes/BasicShapes.BasicShapes")
        self.assertEqual(result["logic"], "30_Dependencies/M0001.pseudo.md")
        self.assertTrue(self.deps("--max-chars", "400")["truncated"])
        self.assertLessEqual(len(self.last_output), 400)

    def test_macro_duplicate_identity_and_corrupt_definition_do_not_fallback(self):
        self.make_macro_definition()
        self.macros[0]["id"] = "G0001"
        self.write_manifest()
        self.invoke("outline", success=False)
        self.make_macro_definition()
        self.set_dependency("/Engine/BasicShapes/BasicShapes.BasicShapes:EngineMacro", "macro_instance")
        (self.root / self.macros[0]["logic"]).write_text("corrupt", encoding="utf-8")
        self.assertEqual(self.deps()["results"][0]["status"], "invalid_export")

    def test_macro_path_duplicates_and_legacy_manifest(self):
        self.make_macro_definition()
        self.macros[0]["path"] = self.records[0]["path"]
        self.write_manifest()
        self.invoke("outline", success=False)
        path = self.root / "01_Manifest.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        del data["macro_definitions"]
        path.write_text(json.dumps(data), encoding="utf-8")
        self.invoke("outline")

    def test_protocol_version_and_outline_macro_opt_in_metadata(self):
        data = json.loads((self.root / "01_Manifest.json").read_text())
        data["pseudo_format_version"] = 2
        (self.root / "01_Manifest.json").write_text(json.dumps(data), encoding="utf-8")
        error = self.invoke("outline", success=False)
        self.assertEqual(error["received"], 2)
        self.assertEqual(error["supported"], [1])
        self.assertEqual(error["field"], "pseudo_format_version")
        data["pseudo_format_version"] = 1
        data["query"] = {"path": "05_Query.py", "protocol_version": 2, "sha1": "x"}
        (self.root / "01_Manifest.json").write_text(json.dumps(data), encoding="utf-8")
        self.invoke("outline", success=False)
        data.pop("query")
        data["macro_definitions"] = [{"id": "M0001", "name": "Macro", "path": "/Engine/A.A:M",
                                       "node_count": 0, "entries": [], "logic": "x", "evidence": "y"}]
        (self.root / "01_Manifest.json").write_text(json.dumps(data), encoding="utf-8")
        result = self.invoke("outline", "--include-macros", "--max-nodes", "1")
        self.assertTrue(result["truncated"])
        self.assertEqual(result["macro_definition_count"], 1)
        complete = self.invoke("outline", "--include-macros")
        self.assertIn("M0001", [item["graph"] for item in complete["results"]])
        self.assertNotIn("M0001", [item["graph"] for item in self.invoke("outline")["results"]])
        self.invoke("outline", "--include-macros", "--max-chars", "400")
        self.assertLessEqual(len(self.last_output), 400)
        self.invoke("find", "--query", "X", "--include-macros", success=False)
        self.invoke("outline", "--graph", "G0001", "--include-macros", success=False)

    def test_protocol_types_contract_hash_and_budget_metadata(self):
        path = self.root / "01_Manifest.json"
        original = json.loads(path.read_text())
        for version in (True, "1", 1.0, None, 2):
            data = {**original, "pseudo_format_version": version}
            path.write_text(json.dumps(data), encoding="utf-8")
            self.invoke("outline", success=False)
        contract_path = self.root / "02_ReadingContract.md"
        contract_path.write_text("阅读契约", encoding="utf-8")
        data = {**original, "pseudo_format_version": 1, "reading_contract": {
            "path": contract_path.name, "version": 1,
            "sha1": hashlib.sha1(contract_path.read_bytes()).hexdigest()}}
        data["graphs"][0].update(logic_bytes=123, evidence_bytes=456)
        path.write_text(json.dumps(data), encoding="utf-8")
        header = self.invoke("outline")["results"][0]
        self.assertEqual((header["logic_bytes"], header["evidence_bytes"]), (123, 456))
        contract_path.write_text("changed", encoding="utf-8")
        self.assertIn("contract hash", self.invoke("outline", success=False)["error"])
        data["reading_contract"]["path"] = "../escape"
        path.write_text(json.dumps(data), encoding="utf-8")
        self.assertIn("escapes", self.invoke("outline", success=False)["error"])

    def test_packaged_query_identity_and_external_compatible_reader(self):
        packaged = self.root / "05_Query.py"
        packaged.write_bytes(TOOL.read_bytes())
        path = self.root / "01_Manifest.json"
        data = json.loads(path.read_text())
        data["query"] = {"path": "05_Query.py", "protocol_version": 1,
                         "sha1": hashlib.sha1(packaged.read_bytes()).hexdigest()}
        path.write_text(json.dumps(data), encoding="utf-8")
        command = [sys.executable, "-B", "-X", "utf8", str(packaged), "outline"]
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        packaged.write_bytes(packaged.read_bytes() + b"\n# different reader copy\n")
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("query script hash mismatch", result.stdout)
        self.invoke("outline")  # Explicit compatible central reader may differ from snapshot reader.

    def test_pseudo_v1_block_grammar_independent_of_evidence(self):
        spec = importlib.util.spec_from_file_location("query_grammar", TOOL)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        text = ('@Outside: ignored()\n```text\n'
                'disabled development_only @N0: call "Run"()\n'
                '  author_comment: "\\n@Fake: not_a_node()\\u0060"\n\n'
                'author_comment @N1 = "comment"\n```\n@Footer: ignored()\n')
        expected = module.logic_blocks(text)
        self.assertEqual(set(expected), {"N0", "N1"})
        self.assertEqual(module.logic_blocks(text.replace("\n", "\r\n")), expected)
        for invalid in (text.replace("```text\n", ""), text.replace("\n```\n", "\n"),
                        text + "```text\n```\n", text.replace('author_comment @N1', 'author_comment @N0'),
                        text.replace('disabled development_only', 'invented'),
                        text.replace('  author_comment:', 'author_comment:')):
            with self.subTest(invalid=invalid):
                with self.assertRaises(ValueError):
                    module.logic_blocks(invalid)


if __name__ == "__main__":
    unittest.main()
