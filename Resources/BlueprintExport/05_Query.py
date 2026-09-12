"""Read-only standard-library queries over a Blueprint ReadPack v2.

Default results contain pseudo statements. Use node --evidence when pin-level
evidence is needed. A slice describes reachability, not runtime evaluation order.
"""

import argparse
from collections import deque
import hashlib
import json
import os
from pathlib import Path
import re


def encode(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def safe(base, relative):
    path = (base / relative).resolve()
    if Path(relative).is_absolute() or not path.is_relative_to(base.resolve()):
        raise ValueError("path escapes query directory")
    return path


def manifest(base):
    data = json.loads((base / "01_Manifest.json").read_text(encoding="utf-8-sig"))
    if data.get("format_version") != 2:
        raise ValueError("unsupported manifest format_version")
    records = data["graphs"]
    for key in ("id", "path"):
        if len({g[key] for g in records}) != len(records):
            raise ValueError("duplicate manifest graph " + key)
    return data


def graphs(data, selector):
    records = data["graphs"]
    if selector is None:
        return records
    matches = [g for g in records if selector in (g["id"], g["path"])]
    if not matches:
        matches = [g for g in records if g["name"] == selector]
    if len(matches) > 1:
        raise ValueError("non-unique graph name: " + selector)
    if not matches:
        raise ValueError("graph not found: " + selector)
    return matches


def logic_blocks(text):
    blocks, current = {}, None
    in_code = False
    for line in text.splitlines():
        if line == "```text":
            in_code = True
            continue
        if not in_code:
            continue
        if line == "```":
            break
        match = re.match(r"^(?:(?:disabled|development_only) )*@([^:\s]+):", line)
        comment = re.match(r"^author_comment @([^\s]+) =", line)
        if match or comment:
            current = (match or comment).group(1)
            if current in blocks:
                raise ValueError("duplicate pseudo node: " + current)
            blocks[current] = [line]
        elif current is not None:
            blocks[current].append(line)
    return {node_id: "\n".join(lines).rstrip() for node_id, lines in blocks.items()}


def graph_data(base, record):
    contents = {}
    for key in ("logic", "evidence"):
        content = safe(base, record[key]).read_bytes()
        expected = record.get(key + "_sha1")
        if not expected:
            raise ValueError("missing " + key + "_sha1")
        if hashlib.sha1(content).hexdigest() != expected:
            raise ValueError("stale hash: " + record[key])
        contents[key] = content.decode("utf-8-sig")
    evidence = json.loads(contents["evidence"])
    if evidence.get("path") != record["path"]:
        raise ValueError("evidence path mismatch")
    nodes = {n["id"]: n for n in evidence["nodes"]}
    if len(nodes) != len(evidence["nodes"]) or len(nodes) != record["node_count"]:
        raise ValueError("evidence node coverage mismatch")
    blocks = logic_blocks(contents["logic"])
    if set(blocks) != set(nodes):
        raise ValueError("pseudo node coverage mismatch")
    return nodes, evidence["edges"], blocks


def ref(edge, side):
    return edge[side + "_node"].get("node_id", "")


def compact_edge(edge):
    return {"kind": edge["kind"],
            "from": [ref(edge, "from"), edge["from_pin_index"]],
            "to": [ref(edge, "to"), edge["to_pin_index"]]}


def dependency_targets(node):
    """Return (kind, raw_target) for the two dependency semantic kinds."""
    semantic = node.get("semantic") or {}
    kind = semantic.get("kind", "")
    if kind not in ("call_function", "macro_instance"):
        return []
    key = "resolved_function" if kind == "call_function" else "macro_graph"
    return [(kind, semantic.get(key) or "")]


def normalize_blueprint_target(raw, kind):
    if raw.startswith("/Script/"):
        return {"raw_target": raw, "status": "native_implementation"}
    match = re.fullmatch(r"(/[^.:]+)\.([^.:/]+):([^:]+)", raw)
    if not match:
        return {"raw_target": raw, "status": "unresolved"}
    package, object_name, member = match.groups()
    name = package.rsplit("/", 1)[-1]
    valid_names = (name,) if kind == "macro_instance" else (name + "_C", "SKEL_" + name + "_C")
    if object_name not in valid_names:
        return {"raw_target": raw, "status": "unresolved"}
    asset_path = package + "." + name
    return {"raw_target": raw, "asset_path": asset_path,
            "graph_path": asset_path + ":" + member}


def candidate_manifests(base):
    """Scan only the current asset directory and its parent's immediate children."""
    roots = [base]
    parent = base.parent
    if parent != base:
        roots.extend(p for p in parent.iterdir() if p.is_dir())
    found = []
    for root in roots:
        manifest_path = root / "01_Manifest.json"
        if (root.resolve().is_relative_to(parent) and manifest_path.is_file()
                and root.resolve() not in found):
            found.append(root.resolve())
    return found


def dependency_index(base):
    index = []
    for root in candidate_manifests(base):
        try:
            index.append((root, manifest(root)))
        except (OSError, ValueError, KeyError, TypeError, UnicodeError):
            index.append((root, None))
    return index


def resolve_dependency(base, raw, kind, index):
    target = normalize_blueprint_target(raw, kind)
    if "status" in target:
        return target
    canonical_name = (target["asset_path"].rsplit(".", 1)[1] + "_"
                      + hashlib.sha1(target["asset_path"].encode("utf-8")).hexdigest())
    matches = [(root, data) for root, data in index
               if data and data.get("asset_path") == target["asset_path"]]
    canonical = [(root, data) for root, data in index if root.name == canonical_name]
    pool = canonical or matches
    if len(pool) != 1:
        target["status"] = "not_exported" if not pool else "ambiguous"
        return target
    root, data = pool[0]
    target["status"] = "invalid_export"
    if not data or data.get("asset_path") != target["asset_path"]:
        return target
    records = [g for g in data["graphs"] if g.get("path") == target["graph_path"]]
    if not records:
        target["status"] = "graph_not_exported"
        return target
    record = records[0]
    try:
        graph_data(root, record)
        if not safe(root, "00_START_HERE.md").is_file():
            return target
    except (OSError, ValueError, KeyError, TypeError, UnicodeError):
        return target
    relative = lambda path: os.path.relpath(path, base).replace(os.sep, "/")
    target.update({"status": "ok", "graph": record["id"],
                   "entry": relative(root / "00_START_HERE.md"),
                   "logic": relative(safe(root, record["logic"])),
                   "evidence": relative(safe(root, record["evidence"])),
                   "query_directory": relative(root),
                   "query_args": ["outline", "--directory", relative(root), "--graph", record["id"]]})
    return target


def dependency_hint(record, node_id):
    return "python 05_Query.py deps --graph " + record["id"] + " --node " + node_id


def selection(seed, nodes, edges):
    execution, incoming = {}, {}
    for edge in edges:
        source, target = ref(edge, "from"), ref(edge, "to")
        if edge["kind"] == "exec":
            execution.setdefault(source, []).append(target)
        elif edge["kind"] == "data":
            incoming.setdefault(target, []).append(source)

    def expand(order, seen, adjacency):
        queue = deque(order)
        while queue:
            for node_id in adjacency.get(queue.popleft(), []):
                if node_id in nodes and node_id not in seen:
                    seen.add(node_id)
                    order.append(node_id)
                    queue.append(node_id)

    order, seen = [seed], {seed}
    expand(order, seen, execution)
    execution_ids = set(seen)
    expand(order, seen, incoming)
    return order, execution_ids


def fits(result, max_chars):
    # Includes print's newline. Characters are not bytes or tokenizer tokens.
    return len(encode(result)) + 1 <= max_chars


def bounded_results(command, items, args, identity):
    kept = items[:args.max_nodes]
    while True:
        result = {"command": command, **identity, "results": kept,
                  "truncated": len(kept) < len(items), "remaining_results": len(items) - len(kept)}
        if not kept and items:
            result["hint"] = "Increase --max-chars or narrow the query."
        if fits(result, args.max_chars):
            return result
        if not kept:
            raise ValueError("budget too small for JSON envelope")
        kept = kept[:-1]


def slice_result(record, seed, nodes, edges, blocks, args, identity):
    order, execution_ids = selection(seed, nodes, edges)
    count = min(len(order), args.max_nodes)
    sample_limit = 8
    while True:
        kept_ids = order[:count]
        kept = set(kept_ids)
        internal, boundary = [], []
        for edge in edges:
            source, target = ref(edge, "from") in kept, ref(edge, "to") in kept
            if source and target:
                internal.append(compact_edge(edge))
            elif source or target:
                boundary.append(compact_edge(edge))
        statements = []
        for node_id in kept_ids:
            dependency = node_id not in execution_ids
            statement = {"id": node_id, "role": "data_dependency" if dependency else "execution",
                         "logic": blocks[node_id]}
            if dependency_targets(nodes[node_id]):
                statement["dependency_hint"] = dependency_hint(record, node_id)
            if dependency and any(p.get("is_exec") for p in nodes[node_id]["pins"]):
                statement["requires_prior_execution"] = True
            statements.append(statement)
        result = {"command": "slice", **identity, "graph": record["id"], "graph_path": record["path"],
                  "seed": seed, "nodes": statements, "edges": internal,
                  "boundary_edges": boundary[:sample_limit], "boundary_edge_count": len(boundary),
                  "omitted_boundary_edges": max(0, len(boundary) - sample_limit),
                  "remaining_nodes": len(order) - count,
                  "truncated": count < len(order) or len(boundary) > sample_limit,
                  "scope": "exec reachability plus upstream data; no runtime-order guarantee"}
        if not count:
            result["hint"] = "Seed does not fit. Increase --max-chars or use node --evidence with a larger budget."
        if fits(result, args.max_chars):
            return result
        if sample_limit:
            sample_limit = 0
        elif count:
            count -= 1
        else:
            raise ValueError("budget too small for JSON envelope")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("outline", "find", "node", "slice", "deps"))
    parser.add_argument("--directory", type=Path, default=Path(__file__).parent)
    parser.add_argument("--graph", help="graph ID, full path, or unique name")
    parser.add_argument("--query")
    parser.add_argument("--node")
    parser.add_argument("--evidence", action="store_true", help="raw evidence for the node command only")
    parser.add_argument("--max-nodes", type=int, default=40, help="node/result count budget")
    parser.add_argument("--max-chars", type=int, default=16000, help="final JSON character budget")
    args = parser.parse_args(argv)
    try:
        if args.max_nodes < 1 or args.max_chars < 256:
            raise ValueError("--max-nodes must be positive; --max-chars must be at least 256")
        if args.evidence and args.command != "node":
            raise ValueError("--evidence is supported only by node")
        if args.command in ("node", "slice") and (not args.graph or not args.node):
            raise ValueError("--graph and --node are required")
        if args.command == "deps" and not args.graph:
            raise ValueError("--graph is required")
        if args.command == "find" and not args.query:
            raise ValueError("--query is required")
        base = args.directory.resolve()
        data = manifest(base)
        identity = {"asset_path": data["asset_path"], "snapshot_id": data["snapshot_id"]}
        records = graphs(data, args.graph)
        index = dependency_index(base) if args.command == "deps" else []
        items = []
        if args.command == "outline":
            # Each graph header and entry is separately budgeted; no graph file is read.
            for record in records:
                items.append({"graph": record["id"], "name": record["name"], "path": record["path"],
                              "node_count": record["node_count"], "entry_count": len(record["entries"])})
                items.extend({"graph": record["id"], "entry": entry} for entry in record["entries"])
        else:
            for record in records:
                nodes, edges, blocks = graph_data(base, record)
                if args.command == "find":
                    for node in nodes.values():
                        if args.query.casefold() in encode(node).casefold():
                            items.append({"graph": record["id"], "id": node["id"], "name": node["name"],
                                          "title": node.get("title", ""), "kind": node.get("semantic", {}).get("kind", "")})
                    continue
                if args.command == "deps":
                    selected = [args.node.lstrip("@")] if args.node else list(nodes)
                    for node_id in selected:
                        if node_id not in nodes:
                            raise ValueError("node not found: " + node_id)
                        for kind, raw in dependency_targets(nodes[node_id]):
                            result = resolve_dependency(base, raw, kind, index)
                            result.update({"node": node_id, "kind": kind})
                            items.append(result)
                    continue
                seed = args.node.lstrip("@")
                if seed not in nodes:
                    raise ValueError("node not found: " + seed)
                if args.command == "slice":
                    print(encode(slice_result(record, seed, nodes, edges, blocks, args, identity)))
                    return 0
                if args.evidence:
                    items.append({"graph": record["id"], "node": nodes[seed],
                                  "edges": [e for e in edges if seed in (ref(e, "from"), ref(e, "to"))]})
                else:
                    item = {"graph": record["id"], "id": seed, "logic": blocks[seed]}
                    if dependency_targets(nodes[seed]):
                        item["dependency_hint"] = dependency_hint(record, seed)
                    items.append(item)
        print(encode(bounded_results(args.command, items, args, identity)))
        return 0
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        print(encode({"error": str(error)}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
