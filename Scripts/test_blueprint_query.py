"""Offline query regressions; no Unreal installation or network required."""
import hashlib
import importlib.util
import contextlib
import io
import json
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

TOOL = Path(__file__).parents[1] / "Resources/BlueprintExport/05_Query.py"


class QueryTests(unittest.TestCase):
    @staticmethod
    def query_module():
        spec = importlib.util.spec_from_file_location("query_performance", TOOL)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def in_process(self, module, *args):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = module.main([*args, "--directory", str(self.root)])
        return code, output.getvalue()

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

    def test_spawn_exposure_survives_node_and_slice_retrieval(self):
        path = self.root / self.records[1]["logic"]
        hint = '  binding: "self_member" expose_on_spawn=true [spawn_argument_possible; default_not_constant]'
        path.write_text('```text\n@N0: read "CoinRotate"()\n' + hint + '\n```\n', encoding="utf-8")
        self.rehash()
        node = self.invoke("node", "--graph", "G0002", "--node", "N0")
        sliced = self.invoke("slice", "--graph", "G0002", "--node", "N0")
        followed = self.invoke("slice", "--graph", "G0002", "--node", "N0", "--follow")
        for result in (node["results"][0], sliced["nodes"][0], followed["results"][0]):
            self.assertIn(hint, result["logic"])

    def test_node_common_fields_and_explicit_evidence_budget_fallback(self):
        record = self.records[1]
        path = self.root / record["evidence"]
        data = json.loads(path.read_text(encoding="utf-8"))
        data["nodes"][0]["large_fact"] = "证据" * 10000
        path.write_text(json.dumps(data), encoding="utf-8")
        self.rehash()
        plain = self.invoke("node", "--graph", "G0002", "--node", "N0")["results"][0]
        full = self.invoke("node", "--graph", "G0002", "--node", "N0", "--evidence", "--max-chars", "100000")
        for key in ("id", "node_guid", "graph", "graph_path", "logic", "logic_status"):
            self.assertEqual(plain[key], full["results"][0][key])
        self.assertEqual(full["results"][0]["node"], data["nodes"][0])
        reduced = self.invoke("node", "--graph", "G0002", "--node", "N0", "--evidence", "--max-chars", "1000")
        self.assertLessEqual(len(self.last_output.rstrip()), 1000)
        self.assertTrue(reduced["truncated"])
        self.assertEqual(reduced["remaining_results"], 0)
        item = reduced["results"][0]
        self.assertEqual((item["id"], item["logic"], item["evidence_status"]), ("N0", plain["logic"], "omitted_budget"))
        self.assertNotIn("node", item)
        restored = self.invoke("node", "--graph", "G0002", "--node", "N0", "--evidence", "--max-chars", str(reduced["required_max_chars"]))
        self.assertFalse(restored["truncated"])
        self.assertEqual(restored["results"][0]["evidence_status"], "included")
        # Even oversized logic can retain identity, without claiming that a missing node was found.
        (self.root / record["logic"]).write_text('```text\n@N0: opaque "' + '大' * 20000 + '"()\n```\n', encoding="utf-8")
        self.rehash()
        minimal = self.invoke("node", "--graph", "G0002", "--node", "N0", "--evidence", "--max-chars", "700")
        self.assertEqual(minimal["results"][0]["id"], "N0")
        self.assertIsNone(minimal["results"][0]["logic"])
        self.assertEqual(minimal["results"][0]["logic_status"], "omitted_budget")
        self.assertTrue(minimal["truncated"])
        self.assertLessEqual(len(self.last_output.rstrip()), 700)
        self.invoke("node", "--graph", "G0002", "--node", "Missing", "--evidence", success=False)

    def test_node_budget_keeps_multiple_identities_and_does_not_mutate_input(self):
        module = self.query_module()
        items = [{"id": f"N{i}", "node_guid": None, "logic": "逻辑" * 50, "logic_status": "included",
                  "node": {"fact": "X" * 5000}, "edges": [], "evidence_status": "included"} for i in range(2)]
        args = SimpleNamespace(max_nodes=2, max_chars=1000)
        result = module.bounded_results("node", items, args, {})
        self.assertEqual([n["id"] for n in result["results"]], ["N0", "N1"])
        self.assertEqual(result["remaining_results"], 0)
        self.assertTrue(result["truncated"])
        self.assertTrue(all(n["evidence_status"] == "omitted_budget" for n in result["results"]))
        self.assertTrue(all(n["evidence_status"] == "included" and "node" in n for n in items))
        self.assertLessEqual(len(module.encode(result)), 1000)

    def test_navigation_arguments_ignore_cwd_and_refresh_after_relocation(self):
        self.set_dependency("/Game/库/Target.Target_C:Sum")
        target = self.target_export()
        (target / "05_Query.py").write_bytes(TOOL.read_bytes())
        navigation = self.deps()["results"][0]
        self.assertEqual(Path(navigation["path_base"]), self.root.resolve())
        self.assertEqual(Path(navigation["query_args"][2]), target.resolve())
        for cwd in (self.root, self.root.parent, TOOL.parent):
            run = subprocess.run([sys.executable, "-B", "-X", "utf8", navigation["query_script"], *navigation["query_args"]],
                                 cwd=cwd, capture_output=True, text=True, encoding="utf-8")
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertEqual(json.loads(run.stdout)["asset_path"], "/Game/库/Target.Target")
        moved = self.root.parent / "搬迁 包"
        moved.mkdir()
        shutil.copytree(target, moved / target.name)
        shutil.copytree(self.root, moved / self.root.name)
        result = self.query_module().resolve_dependency(moved / self.root.name, "/Game/库/Target.Target_C:Sum", "call_function",
            self.query_module().dependency_index(moved / self.root.name))
        self.assertEqual(Path(result["query_args"][2]), (moved / target.name).resolve())
        self.assertEqual(result["query_directory"], navigation["query_directory"])

    def test_nested_follow_target_exposes_its_own_package_base(self):
        self.make_macro_definition()
        self.set_dependency("/Game/库/Target.Target_C:Sum")
        target = self.target_export()
        shutil.copytree(self.root / "30_Dependencies", target / "30_Dependencies")
        (target / "05_Query.py").write_bytes(TOOL.read_bytes())
        data = json.loads((target / "01_Manifest.json").read_text(encoding="utf-8"))
        data["macro_definitions"] = self.macros
        data["graphs"][0]["entries"] = [{"id": "N0"}]
        evidence = json.loads((target / "evidence.json").read_text(encoding="utf-8"))
        evidence["nodes"][0]["semantic"] = {"kind": "macro_instance", "macro_graph": self.macros[0]["path"]}
        (target / "evidence.json").write_text(json.dumps(evidence), encoding="utf-8")
        data["graphs"][0]["evidence_sha1"] = hashlib.sha1((target / "evidence.json").read_bytes()).hexdigest()
        (target / "01_Manifest.json").write_text(json.dumps(data), encoding="utf-8")
        followed = self.invoke("slice", "--graph", "G0001", "--node", "N0", "--follow", "--max-chars", "100000")
        call = next(item for item in followed["results"] if item.get("type") == "call" and item["depth"] == 1)
        self.assertEqual(Path(followed["path_base"]), self.root.resolve())
        self.assertEqual(Path(call["target"]["path_base"]), target.resolve())
        self.assertEqual(call["target"]["query_directory"], ".")
        run = subprocess.run([sys.executable, "-B", "-X", "utf8", call["target"]["query_script"], *call["target"]["query_args"]],
                             cwd=self.root, capture_output=True, text=True, encoding="utf-8")
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertEqual(json.loads(run.stdout)["asset_path"], "/Game/库/Target.Target")

    def test_outline_budget_units_and_totals_are_explicit(self):
        result = self.invoke("outline", "--max-nodes", "1")
        self.assertEqual((result["graph_count"], result["entry_count"]), (2, 2))
        self.assertEqual(result["result_budget_unit"], "graph_header_or_entry")
        self.assertEqual(result["remaining_results"], 3)

    def test_result_prefix_matches_full_collection_bytes_and_counts(self):
        module = self.query_module()

        def legacy(items, args, identity):
            kept = items[:args.max_nodes]
            while True:
                result = {"command": "assets", **identity, "results": kept,
                          "truncated": len(kept) < len(items),
                          "remaining_results": len(items) - len(kept)}
                if not kept and items:
                    result["hint"] = "Increase --max-chars or narrow the query."
                if module.fits(result, args.max_chars):
                    return module.encode(result) + "\n"
                if not kept:
                    raise ValueError("budget too small for JSON envelope")
                kept = kept[:-1]

        for size in (0, 1, 40, 10000):
            source = [{"id": index, "title": "中文" * (index % 17 + 1)} for index in range(size)]
            for limit in (1, 7, 40):
                items = module.ResultItems(limit)
                for item in source:
                    items.append(item)
                    self.assertLessEqual(len(items.kept), limit)
                self.assertEqual(items.total, size)
                for chars in (256, 400, 800, 16000):
                    args = SimpleNamespace(max_nodes=limit, max_chars=chars)
                    identity = {"asset_path": "路径", "snapshot_id": "fixture"}
                    expected = legacy(source, args, identity)
                    actual = module.bounded_results("assets", items.kept, args, identity, items.total)
                    self.assertEqual(module.encode(actual) + "\n", expected)
        args = SimpleNamespace(max_nodes=1, max_chars=256)
        identity = {"asset_path": "too long" * 100}
        with self.assertRaisesRegex(ValueError, "budget too small for JSON envelope"):
            module.bounded_results("assets", [], args, identity, 10000)

    def test_large_single_node_assets_retains_only_budgeted_prefix(self):
        module = self.query_module()
        pins = [{"name": str(i), "direction": "input", "type": {"category": "softobject"},
                 "default": "/Game/Asset.Asset"} for i in range(10000)]
        pins.extend([{"direction": "input", "type": {"category": "softobject"}, "default": "bad"},
                     {"direction": "input", "type": {"category": "string"}, "default": "text"}])
        path = self.root / self.records[0]["evidence"]
        data = json.loads(path.read_text(encoding="utf-8"))
        data["nodes"][0]["pins"] = pins
        path.write_text(json.dumps(data), encoding="utf-8")
        self.rehash()
        original = module.ResultItems
        collectors = []

        class ObservedItems(original):
            def __init__(self, limit):
                super().__init__(limit)
                collectors.append(self)

            def append(self, item):
                super().append(item)
                self.peak = max(getattr(self, "peak", 0), len(self.kept))

        with mock.patch.object(module, "ResultItems", ObservedItems):
            code, output = self.in_process(module, "assets", "--graph", "G0001", "--max-nodes", "3")
        self.assertEqual(code, 0, output)
        result = json.loads(output)
        self.assertEqual(result["remaining_results"], 9997)
        self.assertEqual([item["pin"] for item in result["results"]], ["0", "1", "2"])
        self.assertEqual((result["metadata"]["unresolved"], result["metadata"]["unsupported"]), (1, 1))
        self.assertEqual((collectors[0].peak, collectors[0].total), (3, 10000))

    def test_deps_reuses_validation_but_rechecks_on_next_main(self):
        module = self.query_module()
        self.set_dependency("/Game/库/Target.Target_C:Sum")
        path = self.root / self.records[0]["evidence"]
        data = json.loads(path.read_text(encoding="utf-8"))
        for node in data["nodes"]:
            node["semantic"] = {"kind": "call_function", "resolved_function": "/Game/库/Target.Target_C:Sum"}
        path.write_text(json.dumps(data), encoding="utf-8")
        self.rehash()
        target = self.target_export()
        with mock.patch.object(module, "manifest", wraps=module.manifest) as manifests, \
                mock.patch.object(module, "graph_data", wraps=module.graph_data) as validations:
            code, output = self.in_process(module, "deps", "--graph", "G0001")
            self.assertEqual(code, 0, output)
            self.assertEqual([item["status"] for item in json.loads(output)["results"]], ["ok"] * 7)
            self.assertEqual(manifests.call_count, 2)  # Caller once, target once.
            self.assertEqual(validations.call_count, 2)
            (target / "logic.md").write_text("corrupt", encoding="utf-8")
            code, output = self.in_process(module, "deps", "--graph", "G0001")
            self.assertEqual(code, 0, output)
            self.assertEqual([item["status"] for item in json.loads(output)["results"]], ["invalid_export"] * 7)
            self.assertEqual(manifests.call_count, 4)
            self.assertEqual(validations.call_count, 4)  # Failed validation also reused in this invocation.
            (target / "01_Manifest.json").write_text("corrupt", encoding="utf-8")
            code, output = self.in_process(module, "deps", "--graph", "G0001")
            self.assertEqual(code, 0, output)
            self.assertEqual([item["status"] for item in json.loads(output)["results"]], ["invalid_export"] * 7)
            self.assertEqual(manifests.call_count, 6)

    def test_validation_keys_include_directory_and_full_record_identity(self):
        module = self.query_module()
        target = self.target_export()
        data = json.loads((target / "01_Manifest.json").read_text(encoding="utf-8"))
        record = data["graphs"][0]
        cache = module.GraphValidation()
        with mock.patch.object(module, "graph_data", wraps=module.graph_data) as validations:
            self.assertTrue(cache.valid(target, record))
            self.assertTrue(cache.valid(target / ".", dict(record)))
            self.assertEqual(validations.call_count, 1)
            self.assertFalse(cache.valid(self.root, record))
            self.assertEqual(validations.call_count, 2)
            for key in ("id", "path", "logic", "evidence", "logic_sha1", "evidence_sha1", "node_count"):
                changed = {**record, key: 2 if key == "node_count" else "changed"}
                cache.valid(target, changed)
            self.assertEqual(validations.call_count, 9)
            self.assertTrue(all(type(value) is bool for value in cache.outcomes.values()))

    def test_same_graph_dependency_reuses_current_validation(self):
        module = self.query_module()
        (self.root / "00_START_HERE.md").write_text("entry", encoding="utf-8")
        self.set_dependency("/Game/Test.Test_C:Event")
        with mock.patch.object(module, "manifest", wraps=module.manifest) as manifests, \
                mock.patch.object(module, "graph_data", wraps=module.graph_data) as validations:
            code, output = self.in_process(module, "deps", "--graph", "G0001", "--node", "N0")
            self.assertEqual(code, 0, output)
            self.assertEqual(json.loads(output)["results"][0]["status"], "ok")
            self.assertEqual(manifests.call_count, 1)
            self.assertEqual(validations.call_count, 1)

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

    def test_guid_selection_snapshot_and_duplicate_rejection(self):
        path = self.root / self.records[0]["evidence"]
        evidence = json.loads(path.read_text())
        guid = "12345678123456781234567812345678"
        next(n for n in evidence["nodes"] if n["id"] == "N0")["node_guid"] = guid
        path.write_text(json.dumps(evidence), encoding="utf-8")
        self.rehash()
        result = self.invoke("node", "--graph", "G0001", "--node-guid", guid, "--snapshot", "fixture")
        self.assertEqual(result["results"][0]["node_guid"], guid)
        self.assertEqual(result["results"][0]["id"], "N0")
        found = self.invoke("find", "--query", guid)["results"]
        self.assertEqual(found[0]["node_guid"], guid)
        sliced = self.invoke("slice", "--graph", "G0001", "--node-guid", guid)
        self.assertEqual(sliced["nodes"][0]["node_guid"], guid)
        self.invoke("node", "--graph", "G0001", "--node-guid", guid[:8], success=False)
        self.invoke("node", "--graph", "G0001", "--node-guid", "0" * 32, success=False)
        self.invoke("node", "--graph", "G0001", "--node", "N0", "--snapshot", "old", success=False)
        evidence["nodes"][0]["node_guid"] = guid
        path.write_text(json.dumps(evidence), encoding="utf-8")
        self.rehash()
        self.assertIn("ambiguous", self.invoke("node", "--graph", "G0001", "--node-guid", guid, success=False)["error"])

    def make_cross_graph_cycle(self):
        (self.root / "00_START_HERE.md").write_text("entry", encoding="utf-8")
        self.set_dependency("/Game/Test.Test_C:Other")
        path = self.root / self.records[1]["evidence"]
        evidence = json.loads(path.read_text())
        evidence["nodes"][0]["semantic"] = {"kind": "call_function", "resolved_function": "/Game/Test.Test_C:Event"}
        path.write_text(json.dumps(evidence), encoding="utf-8")
        self.rehash()

    def test_follow_cycle_depth_graph_budget_and_output_budget(self):
        self.make_cross_graph_cycle()
        result = self.invoke("slice", "--graph", "G0001", "--node", "N0", "--follow")
        self.assertEqual(result["metadata"]["visited_graphs"], 2)
        self.assertEqual({r["graph"] for r in result["results"]}, {"G0001", "G0002"})
        self.assertTrue(any(r.get("traversal") == "already_visited" for r in result["results"]))
        limited = self.invoke("slice", "--graph", "G0001", "--node", "N0", "--follow", "--depth", "0")
        self.assertEqual(limited["metadata"]["depth_boundaries"], 1)
        self.assertTrue(limited["truncated"])
        limited = self.invoke("slice", "--graph", "G0001", "--node", "N0", "--follow", "--max-graphs", "1")
        self.assertEqual(limited["metadata"]["pending_graphs"], 1)
        limited = self.invoke("slice", "--graph", "G0001", "--node", "N0", "--follow", "--max-nodes", "1", "--max-chars", "1500")
        self.assertEqual(len(limited["results"]), 1)
        self.assertGreater(limited["remaining_results"], 0)
        self.assertLessEqual(len(self.last_output), 1500)

    def test_impact_transitive_calls_cycle_and_corrupt_coverage(self):
        self.make_cross_graph_cycle()
        result = self.invoke("impact", "--target", "/Game/Test.Test:Other")
        self.assertEqual([r["distance"] for r in result["results"]], [1, 2])
        self.assertFalse(result["metadata"]["complete_asset_graph"])
        limited = self.invoke("impact", "--target", "/Game/Test.Test_C:Other", "--depth", "1")
        self.assertEqual(len(limited["results"]), 1)
        self.assertTrue(limited["truncated"])
        self.invoke("impact", "--target", "/Game/FX.FX", success=False)
        (self.root / self.records[1]["logic"]).write_text("broken", encoding="utf-8")
        broken = self.invoke("impact", "--target", "/Game/Test.Test:Other")
        self.assertFalse(broken["metadata"]["coverage_complete"])
        self.assertEqual(broken["metadata"]["invalid_exports"], 1)

    def test_index_nested_packages_move_stale_and_escape(self):
        self.set_dependency("/Game/库/Target.Target_C:Sum")
        target = self.target_export()
        nested = self.root.parent / "nested"
        nested.mkdir()
        moved = target.rename(nested / target.name)
        self.assertEqual(self.deps()["results"][0]["status"], "not_exported")
        index_path = self.root.parent / "00_INDEX.json"
        entry = {"directory": moved.relative_to(self.root.parent).as_posix(),
                 "asset_path": "/Game/库/Target.Target", "snapshot_id": "target"}
        payload = {"format_version": 1, "packages": [entry]}
        index_path.write_text(json.dumps(payload), encoding="utf-8")
        self.assertEqual(self.deps()["results"][0]["status"], "ok")
        self.assertEqual(self.deps("--index", str(index_path))["results"][0]["status"], "ok")
        entry["snapshot_id"] = "old"
        index_path.write_text(json.dumps(payload), encoding="utf-8")
        self.assertIn("stale package index", self.invoke("deps", "--graph", "G0001", success=False)["error"])
        entry["directory"] = "../outside"
        index_path.write_text(json.dumps(payload), encoding="utf-8")
        self.assertIn("escapes", self.invoke("deps", "--graph", "G0001", success=False)["error"])

    def test_index_unindexed_directories_are_visible_as_coverage_failures(self):
        missing = self.root.parent / "broken"
        missing.mkdir()
        (self.root.parent / "00_INDEX.json").write_text(json.dumps({
            "format_version": 1, "packages": [], "unindexed_directories": ["broken"]}), encoding="utf-8")
        result = self.invoke("impact", "--target", "/Game/Test.Test:Other")
        self.assertEqual(result["metadata"]["invalid_exports"], 1)

    def test_follow_cross_asset_entry_missing_and_invalid_target(self):
        self.set_dependency("/Game/库/Target.Target_C:Sum")
        target = self.target_export()
        args = ("slice", "--graph", "G0001", "--node", "N0", "--follow")
        missing = self.invoke(*args)
        self.assertTrue(any(r.get("traversal") == "no_entry_candidates" for r in missing["results"]))
        path = target / "01_Manifest.json"
        package = json.loads(path.read_text())
        package["graphs"][0]["entries"] = [{"id": "N0"}]
        path.write_text(json.dumps(package), encoding="utf-8")
        followed = self.invoke(*args)
        self.assertEqual(followed["metadata"]["visited_graphs"], 2)
        self.assertTrue(any(r["asset_path"] == package["asset_path"] and r["type"] == "node"
                            for r in followed["results"]))
        (target / "logic.md").write_text("corrupt", encoding="utf-8")
        broken = self.invoke(*args)
        self.assertTrue(any(r.get("target", {}).get("status") == "invalid_export" for r in broken["results"]))

    def test_index_unindexed_valid_manifest_stays_incomplete(self):
        target = self.target_export()
        (self.root.parent / "00_INDEX.json").write_text(json.dumps({
            "format_version": 1, "packages": [], "unindexed_directories": [target.name]}), encoding="utf-8")
        result = self.invoke("impact", "--target", "/Game/Test.Test:Other")
        self.assertEqual(result["metadata"]["invalid_exports"], 1)
        self.assertFalse(result["metadata"]["coverage_complete"])

    def test_validation_resolves_each_base_once_per_query(self):
        module = self.query_module()
        cache = module.GraphValidation()
        original = Path.resolve
        calls = []
        def counted(path, *args, **kwargs):
            calls.append(path)
            return original(path, *args, **kwargs)
        with mock.patch.object(Path, "resolve", counted):
            cache.key(self.root, self.records[0])
            cache.key(self.root, self.records[1])
        self.assertEqual(calls, [self.root])

    def custom_event_fixture(self):
        self.make_graph("G0003", "EventGraph", ["Start", "Call", "EA", "FromA", "EB", "FromB", "Unused"])
        record = self.records[-1]
        path = self.root / record["evidence"]
        graph = json.loads(path.read_text())
        nodes = {n["id"]: n for n in graph["nodes"]}
        for i, node in enumerate(nodes.values()):
            node["node_guid"] = f"{i+1:032X}"
        for entry in ("EA", "EB", "Unused"):
            nodes[entry]["semantic"] = {"kind": "custom_event", "custom_function_name": entry}
        for caller, entry in (("Call", "EA"), ("FromA", "EB"), ("FromB", "EA")):
            nodes[caller]["semantic"] = {"kind": "call_function", "resolved_function": "/Game/Test.SKEL_Test_C:" + entry,
                "function": {"name": entry, "guid": nodes[entry]["node_guid"], "is_self_context": True}}
        graph["edges"] = [{"kind": "exec", "from_node": {"node_id": a}, "from_pin_index": 1,
                           "to_node": {"node_id": b}, "to_pin_index": 0}
                          for a, b in (("Start", "Call"), ("EA", "FromA"), ("EB", "FromB"))]
        record["entries"] = [{"id": entry} for entry in ("Start", "EA", "EB", "Unused")]
        path.write_text(json.dumps(graph), encoding="utf-8")
        (self.root / "00_START_HERE.md").write_text("entry", encoding="utf-8")
        self.rehash()
        return path, graph, nodes

    def test_custom_event_guid_navigation_and_same_graph_entry_cycle(self):
        _, _, nodes = self.custom_event_fixture()
        dep = self.invoke("deps", "--graph", "G0003", "--node", "Call")["results"][0]
        self.assertEqual((dep["status"], dep["graph"], dep["entry_node"]), ("ok", "G0003", "EA"))
        self.assertEqual(dep["node"], "Call")
        self.assertEqual(dep["entry_node_guid"], nodes["EA"]["node_guid"])
        self.assertEqual(dep["matched_by"], "member_guid")
        self.assertEqual(dep["query_args"][-2:], ["--node", "EA"])
        followed = self.invoke("slice", "--graph", "G0003", "--node", "Start", "--follow", "--max-graphs", "1")
        self.assertEqual(followed["metadata"]["visited_graphs"], 1)
        self.assertEqual(followed["metadata"]["entry_expansions"], 3)
        self.assertEqual({n["id"] for n in followed["results"] if n["type"] == "node"},
                         {"Start", "Call", "EA", "FromA", "EB", "FromB"})
        self.assertTrue(any(n.get("traversal") == "already_visited" for n in followed["results"]))
        limited = self.invoke("slice", "--graph", "G0003", "--node", "Start", "--follow", "--depth", "0")
        self.assertEqual(limited["metadata"]["depth_boundaries"], 1)
        self.assertNotIn("EA", [n.get("id") for n in limited["results"]])

    def parent_event_fixture(self):
        target = self.target_export()
        manifest_path = target / "01_Manifest.json"
        package = json.loads(manifest_path.read_text(encoding="utf-8"))
        record = package["graphs"][0]
        record.update(name="EventGraph", path=package["asset_path"] + ":EventGraph", entries=[{"id": "N0"}])
        evidence_path = target / "evidence.json"
        evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
        evidence["path"] = record["path"]
        event = evidence["nodes"][0]
        event.update(node_guid="12345678123456781234567812345678", is_enabled=False,
                     semantic={"kind": "event", "event": {"name": "ReceiveBeginPlay", "guid": "0" * 32,
                                                          "parent_class": "/Script/Engine.Actor"}})
        self.set_dependency("/Game/库/Target.SKEL_Target_C:ReceiveBeginPlay")
        caller_path = self.root / self.records[0]["evidence"]
        caller_graph = json.loads(caller_path.read_text(encoding="utf-8"))
        caller = next(n for n in caller_graph["nodes"] if n["id"] == "N0")
        caller["class_path"] = "/Script/BlueprintGraph.K2Node_CallParentFunction"
        caller["semantic"]["function"] = {"name": "ReceiveBeginPlay", "guid": event["node_guid"]}
        caller_path.write_text(json.dumps(caller_graph), encoding="utf-8")
        evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
        record["evidence_sha1"] = hashlib.sha1(evidence_path.read_bytes()).hexdigest()
        manifest_path.write_text(json.dumps(package), encoding="utf-8")
        self.rehash()
        return target, caller_path, caller_graph, caller

    def test_parent_event_resolves_implementation_node_guid_and_follows_cross_package(self):
        target, _, _, _ = self.parent_event_fixture()
        dep = self.deps()["results"][0]
        self.assertEqual((dep["status"], dep["target_kind"], dep["entry_node"]), ("ok", "event", "N0"))
        self.assertEqual(dep["matched_by"], "member_guid")
        self.assertFalse(dep["is_enabled"])  # Visibility is not an execution claim.
        self.assertEqual(Path(dep["query_args"][2]), target.resolve())
        followed = self.invoke("slice", "--graph", "G0001", "--node", "N0", "--follow")
        self.assertTrue(any(r.get("type") == "node" and r["asset_path"] == "/Game/库/Target.Target"
                            for r in followed["results"]))
        impact = self.invoke("impact", "--target", dep["callable_path"])
        self.assertTrue(any(r.get("node") == "N0" and r.get("graph") == "G0001" for r in impact["results"]))

    def test_parent_event_guid_mismatch_does_not_fall_back_to_name(self):
        _, path, graph, caller = self.parent_event_fixture()
        for guid, field, expected in (("F" * 32, "reason", "event_guid_mismatch"),
                                      ("0" * 32, "matched_by", "unique_event_name")):
            caller["semantic"]["function"]["guid"] = guid
            path.write_text(json.dumps(graph), encoding="utf-8")
            self.rehash()
            self.assertEqual(self.deps()["results"][0][field], expected)

    def test_event_and_custom_event_share_ambiguity_checks_and_reverse_entry_identity(self):
        path, graph, nodes = self.custom_event_fixture()
        nodes["EA"]["semantic"] = {"kind": "event", "event": {"name": "EA", "guid": "0" * 32}}
        path.write_text(json.dumps(graph), encoding="utf-8")
        self.rehash()
        impact = self.invoke("impact", "--target", "/Game/Test.Test:EB")
        self.assertEqual({(r.get("node"), r.get("distance")) for r in impact["results"]},
                         {("FromA", 1), ("Call", 2), ("FromB", 2)})
        nodes["Unused"]["semantic"]["custom_function_name"] = "EA"
        nodes["Call"]["semantic"]["function"]["guid"] = "0" * 32
        path.write_text(json.dumps(graph), encoding="utf-8")
        self.rehash()
        self.assertEqual(self.invoke("deps", "--graph", "G0003", "--node", "Call")["results"][0]["status"], "ambiguous")

    def test_unresolved_call_has_reason_and_budgeted_impact_diagnostic(self):
        self.make_cross_graph_cycle()
        path = self.root / self.records[0]["evidence"]
        graph = json.loads(path.read_text(encoding="utf-8"))
        node = next(n for n in graph["nodes"] if n["id"] == "Other")
        node.update(class_path="/Script/BlueprintGraph.K2Node_Message", semantic={"kind": "call_function"})
        path.write_text(json.dumps(graph), encoding="utf-8")
        self.rehash()
        dep = self.invoke("deps", "--graph", "G0001", "--node", "Other")["results"][0]
        self.assertEqual((dep["status"], dep["reason"]), ("unresolved", "missing_target"))
        for target in ("/Game/Test.Test:Other", "/Game/Unrelated.Unrelated:Missing"):
            result = self.invoke("impact", "--target", target)
            self.assertFalse(result["metadata"]["coverage_complete"])
            self.assertEqual(result["metadata"]["unresolved_calls"], 1)
            diagnostic = next(r for r in result["results"] if r.get("type") == "coverage_error")
            self.assertEqual((diagnostic["node"], diagnostic["reason"]), ("Other", "missing_target"))
            self.assertIn("deps --graph G0001 --node Other", diagnostic["dependency_hint"])
        limited = self.invoke("impact", "--target", "/Game/Test.Test:Other", "--max-nodes", "1")
        self.assertEqual(len(limited["results"]), 1)
        self.assertEqual(limited["remaining_results"], 2)
        self.assertTrue(limited["truncated"])

    def test_missing_required_file_reports_context_without_platform_errno(self):
        for relative, command in (("01_Manifest.json", ("outline",)),
                                  (self.records[0]["logic"], ("node", "--graph", "G0001", "--node", "N0"))):
            path = self.root / relative
            original = path.read_bytes()
            path.unlink()
            try:
                result = self.invoke(*command, success=False)
                self.assertIn("missing required file:", result["error"])
                self.assertIn(path.name, result["error"])
                self.assertNotIn("Errno", result["error"])
            finally:
                path.write_bytes(original)

    def test_unresolved_diagnostics_keep_exact_counts_after_prefix_truncation(self):
        path = self.root / self.records[0]["evidence"]
        graph = json.loads(path.read_text(encoding="utf-8"))
        for node in graph["nodes"]:
            node["semantic"] = {"kind": "call_function"}
        path.write_text(json.dumps(graph), encoding="utf-8")
        self.rehash()
        result = self.invoke("impact", "--target", "/Game/Test.Test:Unknown", "--max-nodes", "1")
        self.assertEqual(result["metadata"]["unresolved_calls"], len(graph["nodes"]))
        self.assertEqual(result["remaining_results"], len(graph["nodes"]) - 1)
        self.assertEqual(len(result["results"]), 1)
        self.assertEqual(result["results"][0]["type"], "coverage_error")
        self.assertTrue(result["truncated"])

    def test_custom_event_guid_disambiguation_and_no_silent_name_fallback(self):
        path, graph, nodes = self.custom_event_fixture()
        nodes["Unused"]["semantic"]["custom_function_name"] = "EA"
        path.write_text(json.dumps(graph), encoding="utf-8")
        self.rehash()
        dep = lambda: self.invoke("deps", "--graph", "G0003", "--node", "Call")["results"][0]
        self.assertEqual(dep()["entry_node"], "EA")
        nodes["Call"]["semantic"]["function"]["guid"] = "F" * 32
        path.write_text(json.dumps(graph), encoding="utf-8")
        self.rehash()
        self.assertEqual(dep()["reason"], "event_guid_mismatch")
        nodes["Call"]["semantic"]["function"]["guid"] = "0" * 32
        path.write_text(json.dumps(graph), encoding="utf-8")
        self.rehash()
        self.assertEqual(dep()["status"], "ambiguous")
        nodes["Unused"]["semantic"]["custom_function_name"] = "Unused"
        path.write_text(json.dumps(graph), encoding="utf-8")
        self.rehash()
        self.assertEqual(dep()["matched_by"], "unique_custom_event_name")
        nodes["Unused"]["node_guid"] = nodes["EA"]["node_guid"]
        nodes["Call"]["semantic"]["function"]["guid"] = nodes["EA"]["node_guid"]
        path.write_text(json.dumps(graph), encoding="utf-8")
        self.rehash()
        self.assertEqual(dep()["status"], "ambiguous")

    def test_custom_event_damaged_graph_prevents_unique_resolution(self):
        self.custom_event_fixture()
        (self.root / self.records[1]["logic"]).write_text("broken", encoding="utf-8")
        result = self.invoke("deps", "--graph", "G0003", "--node", "Call")["results"][0]
        self.assertEqual((result["status"], result["reason"]), ("invalid_export", "event_index_incomplete"))

    def test_custom_event_index_reused_within_query_and_impact_tracks_entry_owners(self):
        self.custom_event_fixture()
        module = self.query_module()
        with mock.patch.object(module, "graph_data", wraps=module.graph_data) as reads:
            code, output = self.in_process(module, "deps", "--graph", "G0003")
            self.assertEqual(code, 0, output)
            self.assertEqual(reads.call_count, 3)  # Caller plus other two owned graphs, not once per call.
        impact = self.invoke("impact", "--target", "/Game/Test.Test:EB")
        pairs = {(r.get("node"), r.get("distance")) for r in impact["results"]}
        self.assertEqual(pairs, {("FromA", 1), ("Call", 2), ("FromB", 2)})
        first = next(r for r in impact["results"] if r["node"] == "FromA")
        self.assertEqual(first["source_entries"][0]["callable_path"], "/Game/Test.Test:EA")

    def test_graph_budget_does_not_block_already_loaded_graph_other_event(self):
        path, graph, nodes = self.custom_event_fixture()
        target = self.target_export()
        manifest_path = target / "01_Manifest.json"
        package = json.loads(manifest_path.read_text())
        package["graphs"][0]["entries"] = [{"id": "N0"}]
        manifest_path.write_text(json.dumps(package), encoding="utf-8")
        nodes["Call"]["semantic"] = {"kind": "call_function", "resolved_function": "/Game/库/Target.Target_C:Sum"}
        graph["edges"].append({"kind": "exec", "from_node": {"node_id": "Call"}, "from_pin_index": 1,
                               "to_node": {"node_id": "FromA"}, "to_pin_index": 0})
        path.write_text(json.dumps(graph), encoding="utf-8")
        self.rehash()
        result = self.invoke("slice", "--graph", "G0003", "--node", "Start", "--follow", "--max-graphs", "1")
        self.assertEqual(result["metadata"]["visited_graphs"], 1)
        self.assertEqual(result["metadata"]["pending_graphs"], 1)
        self.assertIn("EB", {r.get("id") for r in result["results"]})
        self.assertTrue(result["truncated"])


if __name__ == "__main__":
    unittest.main()
