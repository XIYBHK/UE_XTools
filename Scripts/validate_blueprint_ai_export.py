"""Compare project-asset source inventory, detailed JSON and AI JSONL exports.

Generate the inventory in a development Editor with
XTools.BlueprintExport.ValidateAssets /Game/Path/Blueprint [...].
This validator reads artifacts only; it never loads or saves Unreal assets.
"""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re


def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def parse_ai(path):
    metadata = None
    graphs = {}
    current = None
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        if not line.startswith("{"):
            continue
        value = json.loads(line)
        if "asset_path" in value:
            if metadata is not None:
                raise ValueError("Duplicate asset metadata")
            metadata = value
        elif "path" in value and "node_count" in value:
            if value["path"] in graphs:
                raise ValueError("Duplicate graph metadata")
            current = {"metadata": value, "nodes": [], "edges": []}
            graphs[value["path"]] = current
        elif current is not None and "pins" in value:
            current["nodes"].append(value)
        elif current is not None and "from" in value:
            current["edges"].append(value)
        else:
            raise ValueError(f"Unexpected JSON record: {list(value)}")
    if metadata is None:
        raise ValueError("Missing asset metadata")
    return metadata, graphs


def _q(value):
    # Match ReadPack's JSON string quoting (including its code-fence protection).
    return json.dumps(str(value), ensure_ascii=False, separators=(",", ":")).replace("`", "\\u0060")


def validate_pseudo(graph, logic, *, legacy_labels=False, require_locals=False):
    """Independently validate the semantic records in one ReadPack pseudo file.

    This intentionally parses the stable pseudo grammar and derives expected records
    from graph nodes/edges.  It does not compare the output with a separately
    generated copy of the C++ text.
    """
    errors = []
    nodes = {str(n.get("id")): n for n in graph.get("nodes", [])}
    pins = {(str(n.get("id")), int(p.get("index", -1))): p
            for n in graph.get("nodes", []) for p in n.get("pins", [])}
    incoming = {}
    outgoing = {}
    for edge in graph.get("edges", []):
        f = edge.get("from_node", {}).get("node_id")
        t = edge.get("to_node", {}).get("node_id")
        fi, ti = edge.get("from_pin_index"), edge.get("to_pin_index")
        incoming.setdefault((t, ti), []).append(edge)
        outgoing.setdefault((f, fi), []).append(edge)

    def ref(node_id, index):
        pin = pins.get((node_id, index), {})
        name = pin.get("name", f"unresolved_pin_{index}")
        duplicate = sum(p.get("name") == name for (nid, _), p in pins.items() if nid == node_id) > 1
        label = _q(name) + (f"#{index}" if duplicate else "")
        return f"@{node_id}[{label}]"

    def label(node_id, pin):
        if legacy_labels:
            return _q(pin.get("name", ""))
        return ref(node_id, pin.get("index", -1)).split("[", 1)[1][:-1]

    def source(node_id, index, seen=None):
        seen = set() if seen is None else set(seen)
        key = (node_id, index)
        if key in seen:
            return ref(node_id, index) + " [data_cycle]"
        if len(seen) >= 64:
            return ref(node_id, index) + " [continue_in_evidence]"
        seen.add(key)
        node = nodes.get(str(node_id), {})
        if node.get("semantic", {}).get("kind") == "reroute":
            for pin in node.get("pins", []):
                if pin.get("direction") == "input" and not pin.get("is_exec"):
                    links = incoming.get((node_id, pin.get("index")), [])
                    if len(links) == 1:
                        edge = links[0]
                        return source(edge.get("from_node", {}).get("node_id"), edge.get("from_pin_index"), seen)
        return ref(node_id, index)

    def value(node_id, pin):
        links = incoming.get((node_id, pin.get("index")), [])
        if links:
            values = [source(e.get("from_node", {}).get("node_id"), e.get("from_pin_index")) for e in links]
            return values[0] if len(values) == 1 else "sources(" + ", ".join(values) + ") [not_evaluation_order]"
        if pin.get("connected"):
            return "external_or_unresolved [see_evidence]"
        if pin.get("default_value_ignored"):
            return "default_ignored [see_evidence]"
        if pin.get("sub_pin_indices"):
            return "split_input [see_child_arguments]"
        default = str(pin.get("default", ""))
        category = pin.get("type", {}).get("category", "")
        if ((category == "bool" and default in ("true", "false")) or
                (category in ("int", "int64", "real", "float", "double") and
                 re.fullmatch(r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)", default))):
            return default
        return "serialized(" + _q(default) + ")"

    def successor(node_id, pin):
        targets = [ref(e.get("to_node", {}).get("node_id"), e.get("to_pin_index"))
                   for e in outgoing.get((node_id, pin.get("index")), [])]
        if not targets:
            return "external_or_unresolved" if pin.get("connected") else "unconnected"
        return ", ".join(targets) + (" [fanout; no_implied_order]" if len(targets) > 1 else "")

    def split_fields(text):
        fields, start, depth, quoted, escaped = [], 0, 0, False, False
        for i, char in enumerate(text):
            if escaped:
                escaped = False
            elif char == "\\" and quoted:
                escaped = True
            elif char == '"':
                quoted = not quoted
            elif not quoted and char in "([":
                depth += 1
            elif not quoted and char in ")]":
                depth -= 1
            elif not quoted and depth == 0 and text.startswith(", ", i):
                fields.append(text[start:i].strip())
                start = i + 2
        tail = text[start:].strip()
        if tail:
            fields.append(tail)
        return fields

    def operation(node):
        semantic = node.get("semantic", {})
        kind = semantic.get("kind")
        simple = {"function_result": "return", "self": "self", "reroute": "reroute",
                  "sequence": "sequence [outputs_dispatch_in_pin_order; not_await]", "branch": "branch"}
        if kind in simple:
            return simple[kind]
        targets = {
            "event": ("event", semantic.get("event", {}).get("name", "")),
            "custom_event": ("event", semantic.get("custom_function_name", "")),
            "function_entry": ("function_entry", semantic.get("function", {}).get("name", "")),
            "variable": ("set" if semantic.get("access") == "set" else "read", semantic.get("variable", {}).get("name", "")),
            "dynamic_cast": ("cast", semantic.get("target_type", "")),
        }
        if kind in targets:
            verb, target = targets[kind]
            return verb + " " + _q(target)
        if kind in ("async_action", "async_task"):
            return "async " + _q(semantic.get("proxy_factory_class", "") + ":" + semantic.get("proxy_factory_function", ""))
        if kind == "macro_instance":
            return "macro " + _q(semantic.get("macro_graph", "")) + " [" + semantic.get("definition_status", "") + "]"
        if kind == "call_function":
            verb = "expr" if semantic.get("is_pure") else "latent_call" if semantic.get("is_latent") else "call"
            name = semantic.get("resolved_function") or semantic.get("function", {}).get("name", "")
            head = verb + " " + _q(name)
            if node.get("class") not in ("K2Node_CallFunction", "K2Node_CallArrayFunction", "K2Node_PromotableOperator"):
                head += " [specialized_class=" + _q(node.get("class_path", "")) + "; see_evidence]"
            for flag in ("server_rpc", "client_rpc", "net_multicast", "reliable"):
                if semantic.get("is_" + flag):
                    head += " [" + flag + "]"
            return head
        return "opaque " + _q(node.get("class_path", "")) + " [see_evidence; do_not_assume_noop]"

    expected = {}
    for node_id, node in nodes.items():
        if node.get("semantic", {}).get("kind") == "comment":
            expected[node_id] = ([], [])
            continue
        args = []
        for pin in node.get("pins", []):
            if pin.get("direction") != "input" or pin.get("is_exec"):
                continue
            qualifiers = ("ref " if pin.get("type", {}).get("is_reference") else "")
            qualifiers += ("const " if pin.get("type", {}).get("is_const") else "")
            qualifiers += ("orphaned " if pin.get("orphaned") else "")
            args.append(qualifiers + label(node_id, pin) + "=" + value(node_id, pin))
        outputs = []
        data_outputs = []
        kind = node.get("semantic", {}).get("kind")
        async_node = kind in ("async_action", "async_task")
        for pin in node.get("pins", []):
            if pin.get("direction") != "output":
                continue
            if pin.get("is_exec"):
                mode = "callback" if async_node and str(pin.get("name", "")).lower() != "then" else "exit"
                outputs.append((mode, label(node_id, pin), successor(node_id, pin)))
            else:
                description = label(node_id, pin)
                if pin.get("parent_pin_index", -1) != -1:
                    description += " child_of " + ref(node_id, pin["parent_pin_index"])
                if not pin.get("connected"):
                    description += " [unconnected]"
                data_outputs.append(("data", description, None))
        expected[node_id] = (args, outputs + data_outputs)

    actual = {}
    blocks = []
    current = None
    header_pattern = r"^((?:(?:disabled|development_only) )*)@([^:\s]+):(.*)$"
    for line in logic.splitlines():
        if line == "```":
            break
        match = re.match(header_pattern, line)
        comment = re.match(r'^author_comment @([^\s]+) = (.*)$', line)
        if match or comment:
            if current:
                blocks.append(current)
            if comment:
                blocks.append((comment.group(1), "comment", comment.group(2), []))
                current = None
            else:
                current = (match.group(2), match.group(1), match.group(3), [])
        elif current:
            current[3].append(line)
    if current:
        blocks.append(current)
    for node_id, status, head, body in blocks:
        if node_id not in nodes:
            errors.append(f"Unknown pseudo node: {node_id}")
            continue
        if node_id in actual:
            errors.append(f"Duplicate pseudo node: {node_id}")
        node = nodes[node_id]
        if status == "comment":
            if node.get("semantic", {}).get("kind") != "comment" or head != _q(node.get("comment", "")):
                errors.append(f"Pseudo comment differs: {node_id}")
            actual[node_id] = ([], [])
            continue
        locals_ = node.get("semantic", {}).get("local_variables", [])
        if require_locals or any("default_source" in local for local in locals_):
            expected_locals = []
            for local in locals_:
                raw = local.get("default")
                origin = local.get("default_source")
                if raw is None or origin != ("type_default" if raw == "" else "explicit"):
                    errors.append(f"Local default origin differs: {node_id}/{local['name']}")
                type_ = local.get("type", {})
                category = type_.get("category")
                effective = None
                if raw == "" and type_.get("container") == "none":
                    if category == "bool":
                        effective = "false"
                    elif category in ("int", "int64", "real", "float", "double"):
                        effective = "0"
                if local.get("effective_default") != effective:
                    errors.append(f"Local effective default differs: {node_id}/{local['name']}")
                if origin == "explicit":
                    initializer = "serialized(" + _q(raw) + ") [explicit]"
                elif origin == "type_default":
                    initializer = "type_default(" + effective + ")" if effective is not None else "type_default [see_evidence]"
                    initializer += " [raw_default=" + _q(raw) + "]"
                else:
                    initializer = "not_recorded [see_evidence]"
                expected_locals.append("local " + _q(local["name"]) + ": " + _q(type_.get("display", "")) + " = " + initializer)
            actual_locals = [line.strip() for line in body if line.strip().startswith("local ")]
            if actual_locals != expected_locals:
                errors.append(f"Pseudo local initialization differs: {node_id}")
        expected_status = ("disabled " if node.get("is_enabled") is False else "")
        expected_status += "development_only " if node.get("enabled_state") == "DevelopmentOnly" else ""
        if status != expected_status:
            errors.append(f"Pseudo enabled state differs: {node_id}")
        # Locate the argument '(' outside quoted names and annotation brackets.
        quoted, escaped, depth, start = False, False, 0, None
        for index, char in enumerate(head):
            if escaped:
                escaped = False
            elif quoted and char == "\\":
                escaped = True
            elif char == '"':
                quoted = not quoted
            elif not quoted and char == '[':
                depth += 1
            elif not quoted and char == ']':
                depth -= 1
            elif not quoted and char == '(' and depth == 0:
                start = index
                break
        arg_match = re.fullmatch(r"\((.*)\)", head[start:]) if start is not None else None
        if start is not None and head[:start].strip() != operation(node):
            errors.append(f"Pseudo operation differs: {node_id}")
        if not arg_match:
            errors.append(f"Missing pseudo argument list: {node_id}")
        args = split_fields(arg_match.group(1)) if arg_match and arg_match.group(1) else []
        outs = []
        for line in body:
            m = re.match(r'\s*(callback|exit) ("(?:[^"\\]|\\.)*"(?:#\d+)?) -> (.*)$', line)
            if m:
                outs.append((m.group(1), m.group(2), m.group(3)))
            else:
                m = re.match(r"\s*data_outputs: (.*)$", line)
                if m:
                    outs.extend(("data", part, None) for part in split_fields(m.group(1)))
        actual[node_id] = (args, outs)

    for node_id, expected_record in expected.items():
        if node_id not in actual:
            errors.append(f"Missing pseudo node: {node_id}")
            continue
        if actual[node_id] != expected_record:
            errors.append(f"Pseudo semantics differ: {node_id}")
    for node_id in set(actual) - set(expected):
        errors.append(f"Unexpected pseudo node: {node_id}")
    return errors


def validate(asset):
    errors = []
    checks = 0

    def check(condition, message):
        nonlocal checks
        checks += 1
        if not condition:
            errors.append(message)

    result = {"asset_path": asset.get("asset_path", asset["requested_path"])}
    try:
        if not asset.get("export_succeeded"):
            raise ValueError(asset.get("error", "Export failed"))
        directory = Path(asset["export_directory"])
        full_directory = directory / "90_Full" if (directory / "00_START_HERE.md").is_file() else directory
        json_files = list(full_directory.glob("*.json"))
        if len(json_files) != 1:
            raise ValueError(f"Expected one detailed JSON in {directory}")
        json_path = json_files[0]
        ai_path = json_path.with_suffix(".ai.md")
        detailed = load(json_path)
        metadata, ai_graphs = parse_ai(ai_path)
        check(metadata == {k: v for k, v in detailed.items() if k != "graphs"}, "Asset metadata differs")
        check(asset["source_graphs"] == asset["source_graphs_after"], "Export mutated loaded source graph")
        check(asset["dirty_before"] == asset["dirty_after"], "Export changed source package dirty state")
        source_graphs = {g["path"]: g for g in asset["source_graphs"]}
        detailed_graphs = {g["path"]: g for g in detailed["graphs"]}
        check(len(detailed_graphs) == len(detailed["graphs"]), "Duplicate detailed graph paths")
        check(set(source_graphs) == set(detailed_graphs) == set(ai_graphs), "Graph coverage differs")
        pack_sizes = {}
        if full_directory != directory:
            entry_path = directory / "00_START_HERE.md"
            entry = entry_path.read_text(encoding="utf-8-sig")
            evidence_metadata = load(directory / "20_Evidence/00_Asset.json")
            check(evidence_metadata == metadata, "Read pack asset evidence differs")
            manifest_path = directory / "01_Manifest.json"
            manifest = load(manifest_path) if manifest_path.exists() else None
            if manifest is not None:
                check(manifest.get("format_version") == 2, "Unsupported manifest version")
                check(manifest.get("asset_path") == metadata["asset_path"], "Manifest asset identity differs")
                check(len(manifest.get("graphs", [])) == len(detailed["graphs"]), "Manifest graph coverage differs")
                check((directory / "05_Query.py").is_file(), "Missing offline query tool")
            check("90_Full/" in entry and ("10_Logic/" in entry or not detailed["graphs"]), "Read order missing")
            logic_bytes = 0
            graph_sizes = []
            for index, graph in enumerate(detailed["graphs"], 1):
                graph_id = f"G{index:04d}"
                logic_file = f"10_Logic/{graph_id}.pseudo.md"
                evidence_file = f"20_Evidence/{graph_id}.json"
                check(logic_file in entry and evidence_file in entry, f"Unlisted graph pair: {graph_id}")
                check(load(directory / evidence_file) == graph, f"Read pack full graph evidence differs: {graph_id}")
                if manifest is not None:
                    record = manifest["graphs"][index - 1]
                    for key, expected_value in (("id", graph_id), ("path", graph["path"]),
                                                ("logic", logic_file), ("evidence", evidence_file),
                                                ("node_count", len(graph["nodes"]))):
                        check(record.get(key) == expected_value, f"Manifest {graph_id}.{key} differs")
                    for key, filename in (("logic_sha1", logic_file), ("evidence_sha1", evidence_file)):
                        check(record.get(key) == hashlib.sha1((directory / filename).read_bytes()).hexdigest(),
                              f"Manifest {graph_id}.{key} differs")
                logic = (directory / logic_file).read_text(encoding="utf-8-sig")
                for node in graph["nodes"]:
                    check(f"@{node['id']}:" in logic or f"author_comment @{node['id']} =" in logic, f"Missing pseudo node: {graph_id}/{node['id']}")
                for pseudo_error in validate_pseudo(graph, logic, legacy_labels=manifest is None,
                                                    require_locals=manifest is not None and "local_initialization" in manifest.get("features", [])):
                    check(False, f"{graph_id}: {pseudo_error}")
                size = (directory / logic_file).stat().st_size
                logic_bytes += size
                graph_sizes.append({"id": graph_id, "name": graph["name"], "logic_bytes": size, "evidence_bytes": (directory / evidence_file).stat().st_size})
            pack_sizes = {"entry_bytes": entry_path.stat().st_size, "logic_bytes": logic_bytes, "graph_sizes": graph_sizes}
        counts = Counter()
        semantics = Counter()
        unclassified = Counter()
        max_fanout = 0
        for path, source in source_graphs.items():
            graph = detailed_graphs[path]
            ai = ai_graphs[path]
            nodes = {n["name"]: n for n in graph["nodes"]}
            ai_nodes = {n["name"]: n for n in ai["nodes"]}
            check(len(nodes) == len(graph["nodes"]), f"Duplicate node name: {path}")
            check(len(ai_nodes) == len(ai["nodes"]), f"Duplicate AI node name: {path}")
            check(set(nodes) == set(ai_nodes) == {n["name"] for n in source["nodes"]}, f"Node coverage: {path}")
            aliases = {n["id"]: n["name"] for n in graph["nodes"]}
            check(len(aliases) == len(nodes), f"Duplicate node alias: {path}")
            for original in source["nodes"]:
                node = nodes[original["name"]]
                ai_node = ai_nodes[original["name"]]
                for field in ("node_guid", "class_path", "comment", "is_enabled"):
                    check(node[field] == original[field], f"Source node {original['name']}.{field}")
                if "local_variables" in original:
                    locals_ = node.get("semantic", {}).get("local_variables", [])
                    check([{k: v.get(k) for k in ("name", "default")} for v in locals_] == original["local_variables"],
                          f"Source local defaults: {original['name']}")
                for field, value in node.items():
                    if field not in ("pos_x", "pos_y", "pins"):
                        check(ai_node.get(field) == value, f"AI node {original['name']}.{field}")
                check(len(node["pins"]) == len(ai_node["pins"]) == len(original["pins"]), f"Pin count: {original['name']}")
                for pin, ai_pin, original_pin in zip(node["pins"], ai_node["pins"], original["pins"]):
                    for field, value in original_pin.items():
                        actual = pin["type"][field] if field in ("is_reference", "is_const") else pin[field]
                        check(actual == value, f"Source pin {original['name']}:{pin['index']}.{field}")
                    expected = {k: v for k, v in pin.items() if k != "linked_to"}
                    external = [r for r in pin["linked_to"] if not r.get("node_id")]
                    if external:
                        expected["external_links"] = external
                    check(ai_pin == expected, f"AI pin {original['name']}:{pin['index']}")
                counts["pins"] += len(node["pins"])
                semantics[node.get("semantic", {}).get("kind", "unclassified")] += 1
                if node["semantic_status"] == "unclassified":
                    unclassified[node["class_path"]] += 1
            original_edges = Counter((e["kind"], e["from_name"], e["from_index"], e["to_name"], e["to_index"]) for e in source["edges"])
            json_edges = Counter((e["kind"], aliases[e["from_node"]["node_id"]], e["from_pin_index"], aliases[e["to_node"]["node_id"]], e["to_pin_index"]) for e in graph["edges"])
            ai_edges = Counter((e["kind"], aliases[e["from"]["node_id"]], e["from"]["pin_index"], aliases[e["to"]["node_id"]], e["to"]["pin_index"]) for e in ai["edges"])
            check(original_edges == json_edges == ai_edges, f"Exact edge multiset: {path}")
            fanout = Counter((e["from_name"], e["from_index"]) for e in source["edges"])
            max_fanout = max(max_fanout, max(fanout.values(), default=0))
            counts["nodes"] += len(nodes)
            counts["edges"] += len(source["edges"])
            counts["exec_edges"] += sum(e["kind"] == "exec" for e in source["edges"])
            counts["data_edges"] += sum(e["kind"] == "data" for e in source["edges"])
        result.update(counts)
        if pack_sizes:
            result["read_pack"] = pack_sizes
        result.update(graphs=len(source_graphs), semantic_counts=dict(semantics), unclassified_classes=dict(unclassified),
                      max_pin_fanout=max_fanout, json_bytes=json_path.stat().st_size, ai_bytes=ai_path.stat().st_size,
                      ai_path=str(ai_path), dirty_before=asset["dirty_before"], dirty_after=asset["dirty_after"])
    except (KeyError, OSError, ValueError, TypeError) as error:
        errors.append(str(error))
    result.update(passed=not errors, checks=checks, errors=errors)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    inventory = load(args.inventory)
    results = [validate(asset) for asset in inventory["assets"]]
    report = {"engine_version": inventory["engine_version"], "assets": results,
              "passed": sum(r["passed"] for r in results), "failed": sum(not r["passed"] for r in results)}
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if results and not report["failed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
