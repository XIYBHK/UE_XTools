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
import uuid


def encode(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def safe(base, relative):
    path = (base / relative).resolve()
    if Path(relative).is_absolute() or not path.is_relative_to(base.resolve()):
        raise ValueError("path escapes query directory")
    return path


class UnsupportedVersion(ValueError):
    def __init__(self, label, received):
        super().__init__("unsupported " + label)
        self.payload = {"error": str(self), "supported": [1],
                        "received": received, "field": label}


def _version(value, label):
    if type(value) is not int or value != 1:
        raise UnsupportedVersion(label, value)


def manifest(base):
    data = json.loads((base / "01_Manifest.json").read_text(encoding="utf-8-sig"))
    if data.get("format_version") != 2:
        raise ValueError("unsupported manifest format_version")
    if "pseudo_format_version" in data:
        _version(data["pseudo_format_version"], "pseudo_format_version")
    query = data.get("query")
    if query is not None:
        _version(query.get("protocol_version"), "query protocol_version")
        query_path = safe(base, query["path"])
        if query_path == Path(__file__).resolve():
            digest = hashlib.sha1(query_path.read_bytes()).hexdigest()
            if digest != query.get("sha1"):
                raise ValueError("query script hash mismatch")
    contract = data.get("reading_contract")
    if contract is not None:
        _version(contract.get("version"), "reading_contract version")
        path = safe(base, contract["path"])
        if hashlib.sha1(path.read_bytes()).hexdigest() != contract.get("sha1"):
            raise ValueError("reading contract hash mismatch")
    records = data["graphs"]
    macros = data.get("macro_definitions", [])
    for key in ("id", "path"):
        all_records = records + macros
        if len({g[key] for g in all_records}) != len(all_records):
            raise ValueError("duplicate manifest record " + key)
    return data


def graphs(data, selector):
    records = data["graphs"]
    if selector is None:
        return records
    matches = [g for g in records + data.get("macro_definitions", [])
               if selector in (g["id"], g["path"])]
    if not matches:
        matches = [g for g in records + data.get("macro_definitions", [])
                   if g["name"] == selector]
    if len(matches) > 1:
        raise ValueError("non-unique graph name: " + selector)
    if not matches:
        raise ValueError("graph not found: " + selector)
    return matches


def logic_blocks(text):
    blocks, current = {}, None
    in_code = False
    fence_count = 0
    for line in text.splitlines():
        if line == "```text":
            fence_count += 1
            if fence_count > 1 or in_code:
                raise ValueError("invalid pseudo code fences")
            in_code = True
            continue
        if not in_code:
            continue
        if line == "```":
            in_code = False
            continue
        match = re.match(r"^(?:(?:disabled|development_only) )*@([^:\s]+):", line)
        comment = re.match(r"^author_comment @([^\s]+) =", line)
        if match or comment:
            current = (match or comment).group(1)
            if current in blocks:
                raise ValueError("duplicate pseudo node: " + current)
            blocks[current] = [line]
        elif line.strip() and (current is None or not line[0].isspace()):
            raise ValueError("invalid pseudo statement or continuation")
        elif current is not None:
            blocks[current].append(line)
    if in_code or fence_count != 1:
        raise ValueError("invalid pseudo code fences")
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


def candidate_manifests(base, index_path=None, loaded=None):
    """Use an explicit/root index when present; otherwise scan immediate siblings."""
    index_path = index_path or base.parent / "00_INDEX.json"
    if index_path.is_file():
        index = json.loads(index_path.read_text(encoding="utf-8-sig"))
        _version(index.get("format_version"), "package index format_version")
        found = [base.resolve()]
        for entry in index["packages"]:
            root = safe(index_path.parent.resolve(), entry["directory"])
            data = loaded[root] if loaded is not None and root in loaded else manifest(root)
            if (data["asset_path"] != entry["asset_path"]
                    or data["snapshot_id"] != entry["snapshot_id"]):
                raise ValueError("stale package index: " + entry["directory"])
            if loaded is not None:
                loaded[root] = data
            if root not in found:
                found.append(root)
        for relative in index.get("unindexed_directories", []):
            root = safe(index_path.parent.resolve(), relative)
            if loaded is not None:
                loaded[root] = None
            if root not in found:
                found.append(root)
        return found
    if index_path.name != "00_INDEX.json":
        raise ValueError("package index not found: " + str(index_path))
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


def dependency_index(base, current_data=None, index_path=None):
    index = []
    loaded = {base: current_data} if current_data is not None else {}
    if index_path is not None and not index_path.is_file():
        raise ValueError("package index not found: " + str(index_path))
    for root in candidate_manifests(base, index_path, loaded):
        try:
            index.append((root, loaded[root] if root in loaded else manifest(root)))
        except (OSError, ValueError, KeyError, TypeError, UnicodeError):
            index.append((root, None))
    return index


class GraphValidation:
    """Per-query validation and callable identities; never retain graph bodies or exceptions."""
    def __init__(self):
        self.outcomes = {}
        self.bases = {}
        self.members = {}

    def key(self, base, record):
        if base not in self.bases:
            self.bases[base] = base.resolve()
        return (self.bases[base], *(record.get(key) for key in
                ("id", "path", "logic", "evidence", "logic_sha1", "evidence_sha1", "node_count")))

    def remember(self, base, record, nodes=None):
        key = self.key(base, record)
        self.outcomes[key] = True
        if nodes is not None:
            # Keep callable identities only, never graph bodies or node dictionaries.
            self.members[key] = [{"id": node["id"], "node_guid": node.get("node_guid"),
                                  "name": node["semantic"].get("custom_function_name", ""),
                                  "is_enabled": node.get("is_enabled")}
                                 for node in nodes.values() if node.get("semantic", {}).get("kind") == "custom_event"]

    def custom_events(self, base, record):
        key = self.key(base, record)
        if self.outcomes.get(key) is False:
            raise ValueError("invalid custom event index: " + record["path"])
        if key not in self.members:
            try:
                nodes, _, _ = graph_data(base, record)
            except (OSError, ValueError, KeyError, TypeError, UnicodeError):
                self.outcomes[key] = False
                raise
            self.remember(base, record, nodes)
        return self.members[key]

    def valid(self, base, record):
        key = self.key(base, record)
        if key not in self.outcomes:
            try:
                nodes, _, _ = graph_data(base, record)
            except (OSError, ValueError, KeyError, TypeError, UnicodeError):
                self.outcomes[key] = False
            else:
                self.remember(base, record, nodes)
        return self.outcomes[key]


def navigation(base, root, record, entry_node=None):
    """Portable relative links with an explicit base; executable arguments never depend on CWD."""
    base, root = base.resolve(), root.resolve()
    relative = lambda path: os.path.relpath(path, base).replace(os.sep, "/")
    arguments = ["slice" if entry_node else "outline", "--directory", str(root), "--graph", record["id"]]
    if entry_node:
        arguments.extend(["--node", entry_node])
    return {"path_base": str(base), "entry": relative(root / "00_START_HERE.md"),
            "logic": relative(safe(root, record["logic"])), "evidence": relative(safe(root, record["evidence"])),
            "query_directory": relative(root), "query_script": str(root / "05_Query.py"), "query_args": arguments}


def resolve_dependency(base, raw, kind, index, current_data=None, validation=None, caller=None):
    if validation is None:
        validation = GraphValidation()
    target = normalize_blueprint_target(raw, kind)
    if "status" in target:
        return target
    if kind == "macro_instance" and current_data is not None:
        declared = [m for m in current_data.get("macro_definitions", [])
                    if m.get("path") == target["graph_path"]]
        if declared:
            record = declared[0]
            target["status"] = "invalid_export"
            try:
                if not validation.valid(base, record) or not safe(base, "00_START_HERE.md").is_file():
                    return target
            except (OSError, ValueError, KeyError, TypeError, UnicodeError):
                return target
            target.update({"status": "ok", "graph": record["id"],
                           **navigation(base, base, record),
                           "export_owner_asset_path": current_data["asset_path"]})
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
    member = None
    if not records and kind == "call_function":
        function = (caller or {}).get("semantic", {}).get("function", {})
        guid = canonical_guid(function.get("guid"))
        name = target["graph_path"].rsplit(":", 1)[-1]
        matches = []
        named_candidates = 0
        try:
            for candidate in data["graphs"]:
                for entry in validation.custom_events(root, candidate):
                    named_candidates += entry["name"].casefold() == name.casefold()
                    if ((guid and canonical_guid(entry["node_guid"]) == guid)
                            or (not guid and entry["name"].casefold() == name.casefold())):
                        matches.append((candidate, entry))
        except (OSError, ValueError, KeyError, TypeError, UnicodeError):
            # A damaged graph could contain another match, so uniqueness is unproven.
            target.update(status="invalid_export", reason="custom_event_index_incomplete")
            return target
        if len(matches) > 1:
            target.update(status="ambiguous", reason="custom_event_identity", matches=len(matches))
            return target
        if matches:
            record, member = matches[0]
            records = [record]
            target.update(callable_path=target["graph_path"], graph_path=record["path"],
                          target_kind="custom_event", entry_node=member["id"], entry_node_guid=member["node_guid"],
                          member_name=member["name"], is_enabled=member["is_enabled"],
                          matched_by="member_guid" if guid else "unique_custom_event_name")
        else:
            target.update(status="unresolved" if guid and named_candidates else "graph_not_exported",
                          reason="custom_event_guid_mismatch" if guid and named_candidates else "graph_or_custom_event_not_found")
            return target
    if not records:
        target["status"] = "graph_not_exported"
        return target
    record = records[0]
    try:
        if not validation.valid(root, record) or not safe(root, "00_START_HERE.md").is_file():
            return target
    except (OSError, ValueError, KeyError, TypeError, UnicodeError):
        return target
    target.update({"status": "ok", "graph": record["id"],
                   **navigation(base, root, record, member["id"] if member else None)})
    return target


def dependency_hint(record, node_id):
    return "python 05_Query.py deps --graph " + record["id"] + " --node " + node_id


def canonical_guid(value):
    try:
        result = uuid.UUID(str(value))
        return result.hex if result.int else None
    except (ValueError, AttributeError):
        return None


def selected_node(nodes, node_id=None, node_guid=None):
    if node_guid is not None:
        guid = canonical_guid(node_guid)
        if guid is None:
            raise ValueError("--node-guid requires a complete nonzero GUID")
        matches = [n["id"] for n in nodes.values() if canonical_guid(n.get("node_guid")) == guid]
        if len(matches) != 1:
            raise ValueError("node GUID is missing or ambiguous: " + node_guid)
        return matches[0]
    result = (node_id or "").lstrip("@")
    if result not in nodes:
        raise ValueError("node not found: " + result)
    return result


_ASSET_PATH = re.compile(r"^(/[\w-]+(?:/[\w-]+)+)\.([\w-]+)(?::([\w.-]+))?$")
_ASSET_WRAPPED = re.compile(r"^(?:[A-Za-z_][A-Za-z0-9_]*|/Script/[A-Za-z_][A-Za-z0-9_]*\.[A-Za-z_][A-Za-z0-9_]*)'([^']+)'$")


def asset_reference(pin):
    """Return a validated typed default reference, or (reason, None)."""
    pin_type = pin.get("type") or {}
    category = pin_type.get("category", pin.get("category", ""))
    container = pin_type.get("container", "none")
    if container not in (None, "", "none"):
        return "unsupported", None
    if category not in ("object", "class", "softobject", "softclass"):
        return "unsupported", None
    value = pin.get("default_object")
    kind = {"object": "hard_object", "class": "hard_class",
            "softobject": "soft_object", "softclass": "soft_class"}[category]
    if not value and category in ("softobject", "softclass"):
        value = pin.get("default")
    if not isinstance(value, str) or not value:
        return "unresolved", None
    if any(ch.isspace() for ch in value):
        return "unresolved", None
    wrapped = _ASSET_WRAPPED.fullmatch(value)
    if wrapped:
        value = wrapped.group(1)
    match = _ASSET_PATH.fullmatch(value)
    if not match or match.group(1).startswith(("/Script/", "/Temp/", "/Memory/", "/Transient/")):
        return "unresolved", None
    return None, (value, kind)


def asset_items(record, nodes, items):
    unresolved, unsupported = 0, 0
    for node_id, node in nodes.items():
        for index, pin in enumerate(node.get("pins", [])):
            if (pin.get("direction", "").casefold() != "input"
                    or pin.get("is_exec")
                    or pin.get("connected") or pin.get("linked_to") or pin.get("default_value_ignored") or pin.get("orphaned")):
                continue
            if not pin.get("default_object") and pin.get("default") in (None, "", "None"):
                continue
            reason, reference = asset_reference(pin)
            if reason == "unsupported":
                if pin.get("default_object") or pin.get("default"):
                    unsupported += 1
                continue
            if reason:
                unresolved += 1
                continue
            path, kind = reference
            items.append({"graph": record["id"], "node": node_id, "pin": pin.get("name", ""),
                          "pin_index": index, "role": "pin_default", "path": path,
                          "reference_kind": kind, "status": "reference_only"})
    return unresolved, unsupported


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


class ResultItems:
    """Count every match while retaining only the prefix that can be returned."""
    def __init__(self, limit):
        self.limit, self.total, self.kept = limit, 0, []

    def append(self, item):
        self.total += 1
        if len(self.kept) < self.limit:
            self.kept.append(item)

    def extend(self, items):
        for item in items:
            self.append(item)


def bounded_results(command, items, args, identity, total=None):
    if total is None:
        total = len(items)
    kept = [dict(item) for item in items[:args.max_nodes]]
    required_max_chars = None
    while True:
        result = {"command": command, **identity, "results": kept,
                  "truncated": len(kept) < total or any(item.get(field) == "omitted_budget"
                      for item in kept for field in ("evidence_status", "logic_status")),
                  "remaining_results": total - len(kept)}
        if required_max_chars is not None:
            result["required_max_chars"] = required_max_chars
        if not kept and total:
            result["hint"] = "Increase --max-chars or narrow the query."
        if fits(result, args.max_chars):
            return result
        if not kept:
            raise ValueError("budget too small for JSON envelope")
        if command == "node":
            # Drop optional evidence before logic, then fall back to identity-only records.
            # Never call a partially returned node complete or confuse it with a missing node.
            if required_max_chars is None:
                required_max_chars = len(encode(result)) + 1
            candidate = next((item for item in reversed(kept) if item.get("evidence_status") == "included"), None)
            if candidate is not None:
                candidate.pop("node", None)
                candidate.pop("edges", None)
                candidate["evidence_status"] = "omitted_budget"
                continue
            candidate = next((item for item in reversed(kept) if item.get("logic_status") == "included"), None)
            if candidate is not None:
                candidate["logic"] = None
                candidate.pop("dependency_hint", None)
                candidate["logic_status"] = "omitted_budget"
                continue
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
                         "node_guid": nodes[node_id].get("node_guid"), "logic": blocks[node_id]}
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


def follow_slice(base, data, record, seed, args, identity, index):
    """Static entry navigation; other event entries in the same graph remain reachable."""
    queue = deque([(base, data, record, [seed], 0)])
    deferred = []
    seen = {(base, record["path"], seed)}
    visited_graphs = set()
    items = ResultItems(args.max_nodes)
    validation = GraphValidation()
    entry_expansions = depth_boundaries = 0
    while queue:
        next_root, _, next_graph, _, _ = queue[0]
        if (next_root, next_graph["path"]) not in visited_graphs and len(visited_graphs) >= args.max_graphs:
            deferred.append(queue.popleft())
            continue
        root, package, graph, seeds, depth = queue.popleft()
        nodes, edges, blocks = graph_data(root, graph)
        validation.remember(root, graph, nodes)
        visited_graphs.add((root, graph["path"]))
        entry_expansions += len(seeds)
        context = {"asset_path": package["asset_path"], "snapshot_id": package["snapshot_id"],
                   "graph": graph["id"], "graph_path": graph["path"], "depth": depth, "entry_nodes": seeds,
                   "query_directory": os.path.relpath(root, base).replace(os.sep, "/")}
        order, included, execution = [], set(), set()
        for entry in seeds:
            selected, reachable = selection(entry, nodes, edges)
            execution.update(reachable)
            for node_id in selected:
                if node_id not in included:
                    included.add(node_id)
                    order.append(node_id)
        for node_id in order:
            node = nodes[node_id]
            item = {"type": "node", **context, "id": node_id,
                    "node_guid": node.get("node_guid"), "logic": blocks[node_id],
                    "role": "execution" if node_id in execution else "data_dependency"}
            if node_id not in execution and any(p.get("is_exec") for p in node.get("pins", [])):
                item["requires_prior_execution"] = True
            items.append(item)
            for kind, raw in dependency_targets(node):
                target = resolve_dependency(root, raw, kind, index, package, validation, node)
                call = {"type": "call", **context, "node": node_id,
                        "node_guid": node.get("node_guid"), "kind": kind, "target": target}
                if target["status"] == "ok":
                    destination = (root / target["query_directory"]).resolve()
                    target_package = package if destination == root else next(
                        m for p, m in index if p == destination)
                    target_graph = graphs(target_package, target["graph"])[0]
                    entries = ([target["entry_node"]] if "entry_node" in target
                               else [e["id"] for e in target_graph["entries"]])
                    unseen = [entry for entry in entries if (destination, target_graph["path"], entry) not in seen]
                    if not entries:
                        call["traversal"] = "no_entry_candidates"
                    elif not unseen:
                        call["traversal"] = "already_visited"
                    elif depth >= args.depth:
                        call["traversal"] = "depth_limit"
                        depth_boundaries += 1
                    else:
                        seen.update((destination, target_graph["path"], entry) for entry in unseen)
                        queue.append((destination, target_package, target_graph, unseen, depth + 1))
                        call["traversal"] = "queued"
                else:
                    call["traversal"] = "unavailable"
                items.append(call)
    pending = list(queue) + deferred
    metadata = {"scope": "static_cross_graph_navigation; not_inlined; no_runtime_order_guarantee",
                "visited_graphs": len(visited_graphs), "entry_expansions": entry_expansions,
                "pending_graphs": len({(p, g["path"]) for p, _, g, _, _ in pending}), "pending_entries": sum(len(s) for _, _, _, s, _ in pending),
                "depth_boundaries": depth_boundaries,
                "traversal_truncated": bool(pending or depth_boundaries),
                "deduplication": "one_visit_per_package_graph_entry; call_sites_retained",
                "result_budget_unit": "node_or_call_record"}
    result = bounded_results("slice", items.kept, args, {**identity, "metadata": metadata}, items.total)
    result["truncated"] |= metadata["traversal_truncated"]
    return result


def impact_result(base, index, args, identity):
    """Reverse static function/macro calls inside the discovered packages only."""
    if ":" not in args.target:
        raise ValueError("--target requires a full graph/function path; asset references are not call edges")
    target = args.target
    normalized = normalize_blueprint_target(target, "call_function")
    if "graph_path" in normalized:
        target = normalized["graph_path"]
    incoming, failures = {}, []
    unresolved = scanned = 0
    for root, package in index:
        if package is None:
            failures.append({"directory": os.path.relpath(root, base), "status": "invalid_export"})
            continue
        for graph in package["graphs"] + package.get("macro_definitions", []):
            try:
                nodes, edges, _ = graph_data(root, graph)
            except (OSError, ValueError, KeyError, TypeError, UnicodeError) as error:
                failures.append({"directory": os.path.relpath(root, base), "graph": graph["id"],
                                 "status": "invalid_export", "error": str(error)})
                continue
            scanned += 1
            entry_regions = []
            for entry in graph["entries"]:
                entry_node = nodes.get(entry["id"])
                if not entry_node:
                    continue
                semantic = entry_node.get("semantic", {})
                custom_name = semantic.get("custom_function_name") if semantic.get("kind") == "custom_event" else None
                entry_path = package["asset_path"] + ":" + custom_name if custom_name else graph["path"]
                selected, _ = selection(entry["id"], nodes, edges)
                entry_regions.append((set(selected), {"node": entry["id"], "node_guid": entry_node.get("node_guid"),
                                                      "callable_path": entry_path}))
            for node in nodes.values():
                for kind, raw in dependency_targets(node):
                    resolved = normalize_blueprint_target(raw, kind)
                    path = resolved.get("graph_path")
                    if resolved.get("status") == "native_implementation":
                        path = raw
                    if not path:
                        unresolved += 1
                        continue
                    incoming.setdefault(path, []).append({
                        "asset_path": package["asset_path"], "snapshot_id": package["snapshot_id"],
                        "graph": graph["id"], "graph_path": graph["path"], "node": node["id"],
                        "node_guid": node.get("node_guid"), "kind": kind,
                        "source_entries": [entry for selected, entry in entry_regions if node["id"] in selected],
                        "query_directory": os.path.relpath(root, base).replace(os.sep, "/")})
    queue, seen = deque([(target, 0)]), {target}
    items = ResultItems(args.max_nodes)
    depth_boundaries = 0
    while queue:
        path, depth = queue.popleft()
        for caller in incoming.get(path, []):
            items.append({**caller, "target": path, "distance": depth + 1,
                          "status": "static_reference; not_runtime_dispatch_proof"})
            for source in dict.fromkeys(entry["callable_path"] for entry in caller["source_entries"]):
                if source not in seen:
                    seen.add(source)
                    if depth + 1 < args.depth:
                        queue.append((source, depth + 1))
                    elif incoming.get(source):
                        depth_boundaries += 1
    metadata = {"scope": "reverse_function_and_macro_calls_in_discovered_exports",
                "complete_asset_graph": False, "scanned_packages": len(index), "scanned_graphs": scanned,
                "invalid_exports": len(failures), "unresolved_calls": unresolved,
                "depth_boundaries": depth_boundaries, "target": target,
                "coverage_complete": not failures and unresolved == 0}
    for failure in failures:
        items.append({"type": "coverage_error", **failure})
    result = bounded_results("impact", items.kept, args, {**identity, "metadata": metadata}, items.total)
    result["truncated"] |= bool(depth_boundaries)
    return result


def _diff_guid(value):
    """UE identity is the full, nonzero 32-hex GUID; never match short prefixes."""
    if isinstance(value, str) and re.fullmatch(r"[0-9a-fA-F]{32}", value) and int(value, 16):
        return value.upper()
    return None


def _diff_load(base, data):
    loaded = []
    for record in data["graphs"]:
        graph_data(base, record)  # Validate hashes, paths, coverage and pseudo syntax.
        content = safe(base, record["evidence"]).read_bytes()
        if hashlib.sha1(content).hexdigest() != record["evidence_sha1"]:
            raise ValueError("stale hash: " + record["evidence"])
        loaded.append((record, json.loads(content.decode("utf-8-sig"))))
    return loaded


def _diff_unique(items, key):
    grouped = {}
    for item in items:
        value = _diff_guid(key(item))
        if value:
            grouped.setdefault(value, []).append(item)
    return {guid: group[0] for guid, group in grouped.items() if len(group) == 1}


def _diff_canonical(value, node_guids):
    """Rewrite only exported node-reference objects, never arbitrary 'id' values."""
    if isinstance(value, dict):
        if "node_id" in value and "node_guid" in value:
            guid = node_guids.get(value["node_id"])
            if not guid or guid != _diff_guid(value["node_guid"]):
                raise ValueError("unresolved_node_reference")
            return {key: (guid if key == "node_guid" else _diff_canonical(item, node_guids))
                    for key, item in value.items() if key != "node_id"}
        return {key: _diff_canonical(item, node_guids) for key, item in value.items()}
    if isinstance(value, list):
        return [_diff_canonical(item, node_guids) for item in value]
    return value


def _diff_node_parts(node, node_guids):
    default_fields = {"default", "default_object", "default_text", "autogenerated_default"}
    pin_identity_fields = {"id", "persistent_guid", "parent_pin_id", "sub_pin_ids"}
    parts = {"layout": {key: node[key] for key in ("pos_x", "pos_y", "width", "height") if key in node},
             "presentation": {key: node[key] for key in ("title", "comment", "name") if key in node},
             "defaults": [], "pin_identity": [], "pins": [], "connections": []}
    excluded = {"id", "node_guid", "pins", "pos_x", "pos_y", "width", "height", "title", "comment", "name"}
    parts["logic"] = _diff_canonical({key: value for key, value in node.items() if key not in excluded}, node_guids)
    for pin in node.get("pins", []):
        parts["defaults"].append({key: value for key, value in pin.items() if key in default_fields})
        parts["pin_identity"].append({key: value for key, value in pin.items() if key in pin_identity_fields})
        parts["pins"].append({key: value for key, value in pin.items()
                              if key not in default_fields | pin_identity_fields | {"linked_to", "connected"}})
        # Preserve link order: multiple exec links must not silently become unordered.
        parts["connections"].append(_diff_canonical(pin.get("linked_to", []), node_guids))
    return parts


def _diff_edges(graph, node_guids):
    result = []
    for edge in graph.get("edges", []):
        item = _diff_canonical(edge, node_guids)
        for side in ("from_node", "to_node"):
            # Titles/classes describe nodes, not the identity of a connection.
            item[side] = {"node_guid": item[side]["node_guid"]}
        result.append(item)
    # Export edge array order follows canvas-derived node order; it is not exec order.
    return sorted(result, key=lambda item: json.dumps(item, sort_keys=True))


def _diff_graph_metadata(graph, node_guids):
    derived = {"nodes", "edges", "name", "path", "graph_guid", "node_count", "edge_count",
               "exec_edge_count", "data_edge_count", "exec_chain", "entry_nodes",
               "orphan_exec_nodes", "unconnected_exec_pins", "unclassified_node_ids"}
    metadata = _diff_canonical({key: value for key, value in graph.items() if key not in derived}, node_guids)
    # Entry membership matters; its canvas-derived enumeration order does not.
    metadata["entry_node_guids"] = sorted(
        _diff_canonical(ref, node_guids)["node_guid"] for ref in graph.get("entry_nodes", []))
    return metadata


def snapshot_diff(base, other, args, identity):
    """Compare old=other to new=base, restricted to owned graph evidence."""
    new_data, old_data = manifest(base), manifest(other)
    if new_data["asset_path"] != old_data["asset_path"]:
        raise ValueError("diff requires identical asset_path; cross-asset rename matching is unsupported")
    old_graphs, new_graphs = _diff_load(other, old_data), _diff_load(base, new_data)
    old_guids = _diff_unique(old_graphs, lambda item: item[1].get("graph_guid"))
    new_guids = _diff_unique(new_graphs, lambda item: item[1].get("graph_guid"))
    pairs, used_old, used_new = [], set(), set()
    for guid in sorted(old_guids.keys() & new_guids.keys()):
        old, new = old_guids[guid], new_guids[guid]
        pairs.append((old, new, "graph_guid"))
        used_old.add(old[0]["path"])
        used_new.add(new[0]["path"])
    old_paths = {item[0]["path"]: item for item in old_graphs if item[0]["path"] not in used_old}
    new_paths = {item[0]["path"]: item for item in new_graphs if item[0]["path"] not in used_new}
    for path in sorted(old_paths.keys() & new_paths.keys()):
        pairs.append((old_paths[path], new_paths[path], "graph_path"))
        used_old.add(path)
        used_new.add(path)
    items = ResultItems(args.max_nodes)
    summary = {"matched_graphs": len(pairs), "added_graphs": 0, "removed_graphs": 0,
               "not_comparable_graphs": 0, "invalid_identity_nodes": 0,
               "matched_nodes": 0, "added_nodes": 0, "removed_nodes": 0,
               "changed_nodes": 0, "layout_only_nodes": 0}
    for side, records, used in (("removed", old_graphs, used_old), ("added", new_graphs, used_new)):
        for record, graph in records:
            if record["path"] not in used:
                summary[side + "_graphs"] += 1
                items.append({"change": "graph_" + side, "graph_path": record["path"],
                              "graph_guid": graph.get("graph_guid"), "node_count": len(graph["nodes"])})
    for (old_record, old_graph), (new_record, new_graph), matched_by in pairs:
        context = {"old_graph": old_record["id"], "new_graph": new_record["id"],
                   "graph_path": new_record["path"], "matched_by": matched_by}
        old_nodes = _diff_unique(old_graph["nodes"], lambda node: node.get("node_guid"))
        new_nodes = _diff_unique(new_graph["nodes"], lambda node: node.get("node_guid"))
        invalid = len(old_graph["nodes"]) + len(new_graph["nodes"]) - len(old_nodes) - len(new_nodes)
        summary["invalid_identity_nodes"] += invalid
        if invalid or old_graph.get("truncated") or new_graph.get("truncated"):
            summary["not_comparable_graphs"] += 1
            items.append({**context, "change": "not_comparable", "invalid_identity_nodes": invalid,
                          "reason": "missing_or_duplicate_node_guid" if invalid else "truncated_graph"})
            continue
        old_ids = {node["id"]: guid for guid, node in old_nodes.items()}
        new_ids = {node["id"]: guid for guid, node in new_nodes.items()}
        try:
            old_parts = {guid: _diff_node_parts(node, old_ids) for guid, node in old_nodes.items()}
            new_parts = {guid: _diff_node_parts(node, new_ids) for guid, node in new_nodes.items()}
            old_edges, new_edges = _diff_edges(old_graph, old_ids), _diff_edges(new_graph, new_ids)
            old_meta, new_meta = _diff_graph_metadata(old_graph, old_ids), _diff_graph_metadata(new_graph, new_ids)
        except (ValueError, KeyError):
            summary["not_comparable_graphs"] += 1
            items.append({**context, "change": "not_comparable", "reason": "unresolved_node_reference"})
            continue
        if old_record["path"] != new_record["path"] or old_graph.get("name") != new_graph.get("name"):
            items.append({**context, "change": "graph_renamed", "old_path": old_record["path"],
                          "old_name": old_graph.get("name"), "new_name": new_graph.get("name")})
        if old_graph.get("graph_guid") != new_graph.get("graph_guid"):
            items.append({**context, "change": "graph_identity_changed", "old_guid": old_graph.get("graph_guid"),
                          "new_guid": new_graph.get("graph_guid")})
        if old_meta != new_meta:
            items.append({**context, "change": "graph_metadata_changed", "fields": sorted(
                key for key in old_meta.keys() | new_meta.keys()
                if key not in old_meta or key not in new_meta or old_meta[key] != new_meta[key])})
        for side, source, target in (("removed", old_nodes, new_nodes), ("added", new_nodes, old_nodes)):
            for guid in sorted(source.keys() - target.keys()):
                summary[side + "_nodes"] += 1
                items.append({**context, "change": "node_" + side, "node_guid": guid,
                              "node": source[guid]["id"], "title": source[guid].get("title", "")})
        for guid in sorted(old_nodes.keys() & new_nodes.keys()):
            summary["matched_nodes"] += 1
            categories = [key for key in old_parts[guid] if old_parts[guid][key] != new_parts[guid][key]]
            if categories:
                summary["changed_nodes"] += 1
                summary["layout_only_nodes"] += categories == ["layout"]
                item = {**context, "change": "node_changed", "node_guid": guid,
                        "old_node": old_nodes[guid]["id"], "new_node": new_nodes[guid]["id"],
                        "categories": categories}
                if "layout" in categories:
                    item.update(old_layout=old_parts[guid]["layout"], new_layout=new_parts[guid]["layout"])
                items.append(item)
        if old_edges != new_edges:
            items.append({**context, "change": "edges_changed", "old_edge_count": len(old_edges),
                          "new_edge_count": len(new_edges), "includes_pin_identity": True})
    metadata = {**identity, "old_snapshot_id": old_data.get("snapshot_id"),
                "new_snapshot_id": new_data.get("snapshot_id"), "scope": "owned_graph_evidence",
                "complete_comparison": not summary["not_comparable_graphs"], "summary": summary,
                "limitations": "No asset/CDO/component or external macro diff; GUID regeneration is not semantic matching; pin IDs are preserved."}
    return bounded_results("diff", items.kept, args, metadata, items.total)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("outline", "find", "node", "slice", "deps", "assets", "impact", "diff"))
    parser.add_argument("--directory", type=Path, default=Path(__file__).parent)
    parser.add_argument("--graph", help="graph ID, full path, or unique name")
    parser.add_argument("--include-macros", action="store_true", help="include macro headers in outline")
    parser.add_argument("--query")
    parser.add_argument("--node")
    parser.add_argument("--node-guid", help="complete node GUID; requires --graph and a unique match")
    parser.add_argument("--snapshot", help="reject a different snapshot before resolving short IDs")
    parser.add_argument("--follow", action="store_true", help="follow static function/macro references for slice")
    parser.add_argument("--depth", type=int, default=3, help="cross-graph depth; impact defaults to three caller hops")
    parser.add_argument("--max-graphs", type=int, default=40, help="graph traversal budget for slice --follow")
    parser.add_argument("--index", type=Path, help="explicit 00_INDEX.json, including nested package directories")
    parser.add_argument("--target", help="full graph/function path for impact")
    parser.add_argument("--against", type=Path, help="old asset pack directory for diff")
    parser.add_argument("--evidence", action="store_true", help="raw evidence for the node command only")
    parser.add_argument("--max-nodes", type=int, default=40, help="node/result count budget")
    parser.add_argument("--max-chars", type=int, default=16000, help="final JSON character budget")
    args = parser.parse_args(argv)
    try:
        if args.max_nodes < 1 or args.max_chars < 256:
            raise ValueError("--max-nodes must be positive; --max-chars must be at least 256")
        if args.depth < 0 or args.max_graphs < 1:
            raise ValueError("--depth must be nonnegative; --max-graphs must be positive")
        if args.node and args.node_guid:
            raise ValueError("choose --node or --node-guid, not both")
        if args.node_guid and (not args.graph or args.command not in ("node", "slice", "deps", "assets")):
            raise ValueError("--node-guid requires --graph and node/slice/deps/assets")
        if args.follow and args.command != "slice":
            raise ValueError("--follow is supported only by slice")
        if args.index and args.command not in ("deps", "impact", "slice"):
            raise ValueError("--index is supported by deps, impact and slice --follow")
        if args.command == "impact" and (not args.target or args.depth < 1):
            raise ValueError("impact requires --target and a positive --depth")
        if args.command == "diff" and not args.against:
            raise ValueError("diff requires --against <old pack directory>")
        if args.command == "diff" and any((args.graph, args.node, args.node_guid, args.query, args.follow, args.target)):
            raise ValueError("diff compares owned graphs; graph/node/query/follow/target selectors are unsupported")
        if args.command == "impact" and any((args.graph, args.node, args.node_guid, args.query, args.against)):
            raise ValueError("impact uses --target across discovered packages; graph/node/query/against selectors are unsupported")
        if args.against and args.command != "diff":
            raise ValueError("--against is supported only by diff")
        if args.target and args.command != "impact":
            raise ValueError("--target is supported only by impact")
        if args.evidence and args.command != "node":
            raise ValueError("--evidence is supported only by node")
        if args.include_macros and args.command != "outline":
            raise ValueError("--include-macros is supported only by outline")
        if args.include_macros and args.graph:
            raise ValueError("--include-macros cannot be combined with --graph")
        if args.command in ("node", "slice") and (not args.graph or not (args.node or args.node_guid)):
            raise ValueError("--graph and --node or --node-guid are required")
        if args.command == "assets" and args.node and not args.graph:
            raise ValueError("--node requires --graph")
        if args.command == "deps" and not args.graph:
            raise ValueError("--graph is required")
        if args.command == "find" and not args.query:
            raise ValueError("--query is required")
        base = args.directory.resolve()
        data = manifest(base)
        if args.snapshot and args.snapshot != data["snapshot_id"]:
            raise ValueError("snapshot mismatch; re-resolve node identity")
        identity = {"asset_path": data["asset_path"], "snapshot_id": data["snapshot_id"]}
        if args.follow or args.command == "impact":
            identity["path_base"] = str(base)
        if args.command == "diff":
            print(encode(snapshot_diff(base, args.against.resolve(), args, identity)))
            return 0
        if args.command == "impact":
            print(encode(impact_result(base, dependency_index(base, data, args.index), args, identity)))
            return 0
        records = graphs(data, args.graph)
        if args.include_macros:
            records = data["graphs"] + data.get("macro_definitions", [])
        index = dependency_index(base, data, args.index) if args.command == "deps" or args.follow else []
        validation = GraphValidation()
        items = ResultItems(args.max_nodes)
        asset_unresolved = asset_unsupported = 0
        if args.command == "outline":
            identity["macro_definition_count"] = len(data.get("macro_definitions", []))
            identity["graph_count"] = len(records)
            identity["entry_count"] = sum(len(record["entries"]) for record in records)
            identity["result_budget_unit"] = "graph_header_or_entry"
            # Each graph header and entry is separately budgeted; no graph file is read.
            for record in records:
                items.append({"graph": record["id"], "name": record["name"], "path": record["path"],
                              "graph_guid": record.get("graph_guid"),
                              "node_count": record["node_count"], "entry_count": len(record["entries"]),
                              **({k: record[k] for k in ("logic_bytes", "evidence_bytes") if k in record})})
                items.extend({"graph": record["id"], "entry": entry} for entry in record["entries"])
        else:
            for record in records:
                nodes, edges, blocks = graph_data(base, record)
                if args.command == "deps":
                    validation.remember(base, record, nodes)
                if args.command == "find":
                    for node in nodes.values():
                        if args.query.casefold() in encode(node).casefold():
                            items.append({"graph": record["id"], "id": node["id"], "name": node["name"],
                                          "graph_path": record["path"], "node_guid": node.get("node_guid"),
                                          "title": node.get("title", ""), "kind": node.get("semantic", {}).get("kind", "")})
                    continue
                if args.command == "deps":
                    selected = [selected_node(nodes, args.node, args.node_guid)] if args.node or args.node_guid else list(nodes)
                    for node_id in selected:
                        if node_id not in nodes:
                            raise ValueError("node not found: " + node_id)
                        for kind, raw in dependency_targets(nodes[node_id]):
                            result = resolve_dependency(base, raw, kind, index, data, validation, nodes[node_id])
                            result.update({"node": node_id, "node_guid": nodes[node_id].get("node_guid"), "kind": kind,
                                           "scope": "direct_dependency_targets",
                                           "hint": "Use assets for typed unconnected input asset references."})
                            items.append(result)
                    continue
                if args.command == "assets":
                    selected_nodes = ([selected_node(nodes, args.node, args.node_guid)] if args.node or args.node_guid else list(nodes))
                    for node_id in selected_nodes:
                        if node_id not in nodes:
                            raise ValueError("node not found: " + node_id)
                        missing, unsupported = asset_items(record, {node_id: nodes[node_id]}, items)
                        asset_unresolved += missing
                        asset_unsupported += unsupported
                    continue
                seed = selected_node(nodes, args.node, args.node_guid)
                if seed not in nodes:
                    raise ValueError("node not found: " + seed)
                if args.command == "slice":
                    result = (follow_slice(base, data, record, seed, args, identity, index) if args.follow
                              else slice_result(record, seed, nodes, edges, blocks, args, identity))
                    print(encode(result))
                    return 0
                item = {"graph": record["id"], "graph_path": record["path"], "id": seed,
                        "node_guid": nodes[seed].get("node_guid"), "logic": blocks[seed], "logic_status": "included",
                        "evidence_status": "included" if args.evidence else "not_requested"}
                if dependency_targets(nodes[seed]):
                    item["dependency_hint"] = dependency_hint(record, seed)
                if args.evidence:
                    item.update(node=nodes[seed], edges=[e for e in edges if seed in (ref(e, "from"), ref(e, "to"))])
                items.append(item)
        if args.command == "assets":
            identity = {**identity, "metadata": {
                "scope": "typed_unconnected_input_defaults",
                "complete_asset_graph": False,
                "unresolved": asset_unresolved,
                "unsupported": asset_unsupported}}
        elif args.command == "deps":
            identity = {**identity, "metadata": {
                "scope": "forward_function_and_macro_calls",
                "complete_asset_graph": False,
                "asset_reference_hint": "python 05_Query.py assets --graph " + records[0]["id"]}}
        print(encode(bounded_results(args.command, items.kept, args, identity, items.total)))
        return 0
    except UnsupportedVersion as error:
        print(encode(error.payload))
        return 1
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        print(encode({"error": str(error)}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
