import json
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
    def test_valid_text_passes(self):
        self.assertEqual(validate_pseudo(fixture(), good_text()), [])

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


if __name__ == "__main__":
    unittest.main()
