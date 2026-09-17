import json
import copy
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from validate_blueprint_ai_export import validate_pseudo


def fixture():
    return {
        "nodes": [
            {"id": "A", "semantic": {"kind": "event", "event": {"name": "Start"}}, "pins": [
                {"index": 0, "name": "then", "direction": "output", "is_exec": True},
            ]},
            {"id": "B", "semantic": {"kind": "async_action", "proxy_factory_class": "Proxy", "proxy_factory_function": "Run"}, "pins": [
                {"index": 0, "name": "execute", "direction": "input", "is_exec": True},
                {"index": 1, "name": "then", "direction": "output", "is_exec": True},
                {"index": 2, "name": "Then", "direction": "output", "is_exec": True},
                {"index": 3, "name": "Value", "direction": "input", "type": {"category": "bool"}, "default": "false"},
                {"index": 4, "name": "Value", "direction": "output", "connected": False},
            ]},
        ],
        "edges": [
            {"kind": "exec", "from_node": {"node_id": "A"}, "from_pin_index": 0,
             "to_node": {"node_id": "B"}, "to_pin_index": 0},
        ],
    }


def good_text():
    return '''```text
@A: event "Start"()
  exit "then" -> @B["execute"]

@B: async "Proxy:Run"("Value"#3=false)
  exit "then" -> unconnected
  exit "Then" -> unconnected
  data_outputs: "Value"#4 [unconnected]
```
'''


class PseudoValidationTests(unittest.TestCase):
    def test_spawn_exposure_hint_matches_declaration_and_scope(self):
        identity = "FA33A0754A87952E34F4E0827334A86D"
        variable = {"name": "CoinRotate", "guid": identity, "metadata": {"ExposeOnSpawn": "true"}}
        reference = {"name": "CoinRotate", "guid": identity, "is_self_context": True}
        semantic = {"kind": "variable", "access": "get", "binding_origin": "self_member", "variable": reference}
        graph = {"nodes": [{"id": "A", "semantic": semantic, "pins": []}], "edges": []}
        base = ('```text\n@A: read "CoinRotate"()\n'
                '  demand: data_edges=0 consumers=0 [static_direct; not_call_count]\n'
                '  binding: "self_member"\n```\n')
        suffix = ' expose_on_spawn=true [spawn_argument_possible; default_not_constant]'
        hinted = base.replace('"self_member"\n', '"self_member"' + suffix + '\n')
        options = dict(semantic_hints=True, spawn_exposure_hints=True, variables=[variable])
        self.assertEqual(validate_pseudo(graph, hinted, **options), [])
        self.assertTrue(validate_pseudo(graph, base, **options))
        self.assertTrue(validate_pseudo(graph, hinted.replace('=true', '=false'), **options))
        for replacements in ({"guid": "0" * 32}, {"guid": "F" * 32}, {"name": "Other"},
                             {"is_self_context": False}, {"is_local_scope": True}):
            modified = copy.deepcopy(graph)
            modified["nodes"][0]["semantic"]["variable"].update(replacements)
            text = base.replace('"CoinRotate"', '"Other"') if "name" in replacements else base
            self.assertEqual(validate_pseudo(modified, text, **options), [])
            self.assertTrue(validate_pseudo(modified, text.replace('"self_member"\n', '"self_member"' + suffix + '\n'), **options))
        for declarations in ([], [variable, variable], [{**variable, "metadata": {"ExposeOnSpawn": "false"}}]):
            self.assertEqual(validate_pseudo(graph, base, **{**options, "variables": declarations}), [])
            self.assertTrue(validate_pseudo(graph, hinted, **{**options, "variables": declarations}))
        # Old packs remain valid and do not silently acquire new claims.
        self.assertEqual(validate_pseudo(graph, base, semantic_hints=True), [])

    def test_iteration_hint_requires_exact_evidence_and_cannot_be_invented(self):
        iteration = {"kind": "array", "loop_body_pin": "LoopBody", "definition_graph": "/Engine/Macros:ForEachLoop"}
        semantic = {"kind": "macro_instance", "macro_graph": "/Engine/Macros:ForEachLoop",
                    "definition_status": "dependency_included", "iteration": iteration}
        graph = {"nodes": [{"id": "A", "semantic": semantic, "pins": []}], "edges": []}
        text = '```text\n@A: macro "/Engine/Macros:ForEachLoop" [dependency_included]()\n  iteration: ' + json.dumps(iteration) + '\n```\n'
        self.assertEqual(validate_pseudo(graph, text, semantic_hints=True), [])
        self.assertTrue(validate_pseudo(graph, text.replace('"LoopBody"', '"Completed"'), semantic_hints=True))
        del semantic["iteration"]
        self.assertTrue(validate_pseudo(graph, text, semantic_hints=True))

    def test_classified_fallback_retains_switch_facts_and_rejects_tampering(self):
        semantic = {"kind": "switch", "cases": ["测试1", "测试2"], "switch_type": "name"}
        graph = {"nodes": [{"id": "A", "title": "切换名称", "class_path": "/Script/BlueprintGraph.K2Node_SwitchName",
                            "semantic": semantic, "pins": []}], "edges": []}
        head = '@A: classified "switch" "切换名称" [class="/Script/BlueprintGraph.K2Node_SwitchName"; see_evidence; not_full_compiler_semantics]()'
        text = '```text\n' + head + '\n  semantic: ' + json.dumps(semantic, ensure_ascii=False) + '\n```\n'
        self.assertEqual(validate_pseudo(graph, text, classified_fallback=True), [])
        for broken in (text.replace('"switch"', '"select"', 1), text.replace('"测试2"', '"Wrong"'),
                       text.replace('  semantic:', '  removed:'), text.replace('切换名称', 'Wrong title'),
                       text.replace('  semantic:', '  semantic: {}\n  semantic:')):
            self.assertTrue(validate_pseudo(graph, broken, classified_fallback=True))
        legacy = '```text\n@A: opaque "/Script/BlueprintGraph.K2Node_SwitchName" [see_evidence; do_not_assume_noop]()\n```\n'
        self.assertEqual(validate_pseudo(graph, legacy), [])
        self.assertTrue(validate_pseudo(graph, legacy, classified_fallback=True))

    def test_unknown_node_still_requires_opaque_and_future_kind_retains_facts(self):
        graph = {"nodes": [{"id": "A", "class_path": "/Script/Test.Special", "pins": []}], "edges": []}
        text = '```text\n@A: opaque "/Script/Test.Special" [see_evidence; do_not_assume_noop]()\n```\n'
        self.assertEqual(validate_pseudo(graph, text, classified_fallback=True), [])
        graph['nodes'][0]['semantic'] = {'kind': 'future_kind', 'observed_value': 42}
        text = ('```text\n@A: classified "future_kind" "" [class="/Script/Test.Special"; see_evidence; not_full_compiler_semantics]()\n'
                '  semantic: {"observed_value":42,"kind":"future_kind"}\n```\n')
        self.assertEqual(validate_pseudo(graph, text, classified_fallback=True), [])

    def test_component_binding_and_static_demand_not_fabricated(self):
        graph = {"nodes": [{"id": "A", "semantic": {"kind": "variable", "access": "get",
            "variable": {"name": "Mesh"}, "component_binding": "scs_property", "binding_origin": "self_member",
            "scs_node_path": "/Game/Test:MeshNode"}, "pins": []}], "edges": []}
        text = ('```text\n@A: component_read "Mesh"()\n'
                '  demand: data_edges=0 consumers=0 [static_direct; not_call_count]\n'
                '  binding: "self_member" component="scs_property" scs="/Game/Test:MeshNode"\n```\n')
        self.assertEqual(validate_pseudo(graph, text, semantic_hints=True), [])
        for old, new in (("consumers=0", "consumers=3"), ("scs_property", "object_property"), ("MeshNode", "Wrong")):
            self.assertTrue(validate_pseudo(graph, text.replace(old, new), semantic_hints=True))

    def test_valid_text_passes(self):
        self.assertEqual(validate_pseudo(fixture(), good_text()), [])

    def test_engine_macro_boolean_spelling_is_preserved(self):
        graph = fixture()
        graph["nodes"][1]["pins"][3]["default"] = "TRUE"
        text = good_text().replace('"Value"#3=false', '"Value"#3=TRUE')
        self.assertEqual(validate_pseudo(graph, text), [])
        self.assertTrue(validate_pseudo(graph, text.replace("=TRUE", "=false")))

    def test_macro_temporary_persistence_and_assignment_are_distinct(self):
        graph = {"nodes": [
            {"id": "A", "semantic": {"kind": "temporary_variable", "is_persistent": True,
                                     "variable_type": {"display": "bool"}}, "pins": []},
            {"id": "B", "semantic": {"kind": "assignment"}, "pins": []}], "edges": []}
        text = ('```text\n@A: temp "bool" [compiler_local; persistent_savegame; no_lifetime_inference]()\n'
                '@B: assign [Variable_is_write_target]()\n```\n')
        self.assertEqual(validate_pseudo(graph, text, semantic_hints=True), [])
        self.assertTrue(validate_pseudo(graph, text.replace("persistent_savegame; ", ""), semantic_hints=True))
        self.assertTrue(validate_pseudo(graph, text.replace("assign", "read"), semantic_hints=True))

    def test_deleted_exec_edge_fails(self):
        text = good_text().replace('exit "then" -> @B["execute"]', 'exit "then" -> unconnected')
        self.assertTrue(validate_pseudo(fixture(), text))

    def test_swapped_callbacks_fails(self):
        text = good_text().replace('exit "Then" -> unconnected', 'exit "Then" -> @B["execute"]')
        self.assertTrue(validate_pseudo(fixture(), text))

    def test_deleted_false_default_fails(self):
        text = good_text().replace('(\"Value\"#3=false)', '(\"Value\"#3=serialized(\"\"))')
        self.assertTrue(validate_pseudo(fixture(), text))

    def test_wrong_operation_or_enabled_state_fails(self):
        self.assertTrue(validate_pseudo(fixture(), good_text().replace('"Proxy:Run"', '"Proxy:Wrong"')))
        self.assertTrue(validate_pseudo(fixture(), good_text().replace('@B:', 'disabled @B:')))

    def test_duplicate_input_labels_and_zero_empty_defaults(self):
        graph = fixture()
        graph["nodes"][1]["pins"].extend([
            {"index": 5, "name": "Arg", "direction": "input", "type": {"category": "int"}, "default": "0"},
            {"index": 6, "name": "Arg", "direction": "input", "type": {"category": "string"}, "default": ""},
        ])
        text = good_text().replace('"Value"#3=false)', '"Value"#3=false, "Arg"#5=0, "Arg"#6=serialized(""))')
        self.assertEqual(validate_pseudo(graph, text), [])
        for token in (', "Arg"#5=0', ', "Arg"#6=serialized("")'):
            self.assertTrue(validate_pseudo(graph, text.replace(token, "")))
        self.assertTrue(validate_pseudo(graph, text.replace('"Arg"#5', '"Arg"#6')))

    def test_data_source_and_quoted_comma(self):
        graph = fixture()
        graph["nodes"][0]["pins"].insert(0, {
            "index": 1, "name": 'Data, "quoted"', "direction": "output", "connected": True})
        graph["edges"].append({"kind": "data", "from_node": {"node_id": "A"}, "from_pin_index": 1,
                               "to_node": {"node_id": "B"}, "to_pin_index": 3})
        quoted = json.dumps('Data, "quoted"')
        text = good_text().replace('"Value"#3=false', '"Value"#3=@A[' + quoted + ']')
        text = text.replace('  exit "then" -> @B["execute"]', '  exit "then" -> @B["execute"]\n  data_outputs: ' + quoted)
        self.assertEqual(validate_pseudo(graph, text), [])
        self.assertTrue(validate_pseudo(graph, text.replace('@A[' + quoted + ']', '@B["Value"#4]')))

    def test_node_presence_alone_is_insufficient(self):
        self.assertTrue(validate_pseudo(fixture(), '```text\n@A: event "Start"()\n@B: async "Proxy:Run"()\n```\n'))

    def test_local_defaults_are_required_and_not_fabricated(self):
        graph = fixture()
        graph["nodes"][0]["semantic"] = {"kind": "function_entry", "function": {"name": "Start"},
            "local_variables": [
                {"name": "Accumulator", "type": {"category": "real", "container": "none", "display": "real:double"},
                 "default": "", "default_source": "type_default", "effective_default": "0"},
                {"name": "Flag", "type": {"category": "bool", "container": "none", "display": "bool"},
                 "default": "false", "default_source": "explicit"},
                {"name": "Value", "type": {"category": "struct", "container": "none", "display": "struct"},
                 "default": "", "default_source": "type_default"}]}
        declarations = ('  local "Accumulator": "real:double" = type_default(0) [raw_default=""]\n'
                        '  local "Flag": "bool" = serialized("false") [explicit]\n'
                        '  local "Value": "struct" = type_default [see_evidence] [raw_default=""]\n')
        text = good_text().replace('@A: event "Start"()\n', '@A: function_entry "Start"()\n' + declarations)
        self.assertEqual(validate_pseudo(graph, text, require_locals=True), [])
        self.assertTrue(validate_pseudo(graph, text.replace(declarations, ""), require_locals=True))
        self.assertTrue(validate_pseudo(graph, text.replace("type_default(0)", "type_default(1)")))
        graph["nodes"][0]["semantic"]["local_variables"][2]["effective_default"] = "0"
        self.assertTrue(validate_pseudo(graph, text))


class TypedFactsTests(unittest.TestCase):
    def operator_fixture(self):
        node = {"id": "A", "class": "K2Node_PromotableOperator", "class_path": "/Script/BlueprintGraph.K2Node_PromotableOperator",
                "semantic": {"kind": "call_function", "is_pure": True, "resolved_function": "Add_VectorVector"},
                "pins": [{"index": 7, "name": "A", "direction": "input", "type": {"category": "real", "display": "real:double"}, "default": "1"},
                         {"index": 2, "name": "ReturnValue", "direction": "output", "type": {"display": "struct/Vector"}}]}
        pins = [{"index": p["index"], "name": p["name"], "direction": p["direction"], "type": p["type"]["display"]} for p in node["pins"]]
        head = (' [specialized_class="/Script/BlueprintGraph.K2Node_PromotableOperator"; see_evidence]'
                ' [promotable_operator; serialized_function_reference; compilation_not_resolved]')
        text = ('```text\n@A: expr "Add_VectorVector"' + head + '("A"=1)\n'
                '  operator_pins: ' + json.dumps(pins) + '\n  data_outputs: "ReturnValue" [unconnected]\n```\n')
        return {"nodes": [node], "edges": []}, text, head

    def test_current_types_retained_without_claiming_compiler_result(self):
        graph, text, _ = self.operator_fixture()
        self.assertEqual(validate_pseudo(graph, text, typed_operator_hints=True), [])
        for broken in (text.replace("real:double", "struct/Vector"), text.replace('"index": 7', '"index": 0'),
                       text.replace("compilation_not_resolved", "compilation_resolved"),
                       '\n'.join(line for line in text.splitlines() if "operator_pins:" not in line)):
            self.assertTrue(validate_pseudo(graph, broken, typed_operator_hints=True))

    def test_old_package_remains_valid(self):
        graph, text, head = self.operator_fixture()
        legacy = '\n'.join(line for line in text.replace(head, "").splitlines() if "operator_pins:" not in line)
        self.assertEqual(validate_pseudo(graph, legacy), [])
        self.assertTrue(validate_pseudo(graph, legacy, typed_operator_hints=True))

    def test_enum_mapping_requires_exact_identity_value_and_display(self):
        graph, text, _ = self.operator_fixture()
        enums = [{"path": "/Game/State.State", "entries": [
            {"internal_name": "NewEnumerator0", "value": "9007199254740993", "display_name": "同名", "hidden": False},
            {"internal_name": "NewEnumerator2", "value": "-7", "display_name": "同名", "hidden": True}]}]
        graph["nodes"][0]["semantic"]["enum_definitions"] = enums
        text = text.replace('  data_outputs:', '  enum_definitions: ' + json.dumps(enums, ensure_ascii=False) + '\n  data_outputs:')
        flags = {"typed_operator_hints": True, "enum_display_mappings": True}
        self.assertEqual(validate_pseudo(graph, text, **flags), [])
        for broken in (text.replace("9007199254740993", "9007199254740992"), text.replace("NewEnumerator2", "NewEnumerator0"),
                       text.replace("同名", "改名"), '\n'.join(line for line in text.splitlines() if "enum_definitions:" not in line)):
            self.assertTrue(validate_pseudo(graph, broken, **flags))

    def test_unknown_node_keeps_mapping_without_claiming_classification(self):
        enums = [{"path": "/Game/State.State", "entries": []}]
        node = {"id": "U", "class_path": "/Script/Other.Unknown", "semantic": {"enum_definitions": enums}, "pins": []}
        graph = {"nodes": [node], "edges": []}
        text = ('```text\n@U: opaque "/Script/Other.Unknown" [see_evidence; do_not_assume_noop]()\n'
                '  enum_definitions: ' + json.dumps(enums) + '\n```\n')
        self.assertEqual(validate_pseudo(graph, text, enum_display_mappings=True, classified_fallback=True), [])
        self.assertTrue(validate_pseudo(graph, text.replace('opaque', 'classified'), enum_display_mappings=True, classified_fallback=True))


if __name__ == "__main__":
    unittest.main()
