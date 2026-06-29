#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Analyze Unreal Engine Blueprint copy/export text and summarize the graph flow.

Typical usage:
  python Scripts/ue_blueprint_export_flow.py Docs/ref/沿样条线移动.txt
  python Scripts/ue_blueprint_export_flow.py Docs/ref/沿样条线移动.txt -o Docs/ref/沿样条线移动.flow.md
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


PIN_LINE_PREFIX = "CustomProperties Pin"


@dataclass
class PinLink:
    node_name: str
    pin_id: str


@dataclass
class Pin:
    pin_id: str
    name: str
    direction: str
    category: str
    sub_category: str = ""
    object_type: str = ""
    default_value: str = ""
    links: list[PinLink] = field(default_factory=list)


@dataclass
class Node:
    name: str
    class_path: str
    class_name: str
    start_line: int
    end_line: int
    body: str
    pos_x: int | None = None
    pos_y: int | None = None
    function_name: str = ""
    function_parent: str = ""
    event_name: str = ""
    event_parent: str = ""
    custom_function_name: str = ""
    delegate_property_name: str = ""
    delegate_owner_class: str = ""
    component_property_name: str = ""
    macro_name: str = ""
    comment: str = ""
    pins: list[Pin] = field(default_factory=list)


def read_text_auto(path: Path) -> str:
    data = path.read_bytes()
    if data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
        return data.decode("utf-16")
    if data[:512].count(b"\x00") > 8:
        return data.decode("utf-16")
    for encoding in ("utf-8-sig", "utf-8", "utf-16"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("mbcs")


def field_value(text: str, key: str) -> str:
    marker = f"{key}="
    start = text.find(marker)
    if start < 0:
        return ""

    i = start + len(marker)
    if i >= len(text):
        return ""

    if text[i] == '"':
        i += 1
        out: list[str] = []
        while i < len(text):
            ch = text[i]
            if ch == '"' and (i == 0 or text[i - 1] != "\\"):
                break
            out.append(ch)
            i += 1
        return "".join(out)

    if text[i] == "(":
        depth = 0
        start_value = i
        while i < len(text):
            ch = text[i]
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    return text[start_value + 1 : i]
            i += 1
        return text[start_value + 1 :]

    start_value = i
    while i < len(text) and text[i] not in ",)":
        i += 1
    return text[start_value:i].strip()


def parse_member_reference(raw: str) -> tuple[str, str]:
    if not raw:
        return "", ""
    parent = field_value(raw, "MemberParent")
    member = field_value(raw, "MemberName")
    if not member:
        member = field_value(raw, "MemberGuid")
    return parent, member


def simplify_object_path(path: str) -> str:
    if not path or path == "None":
        return ""
    quoted = re.search(r"'([^']+)'", path)
    value = quoted.group(1) if quoted else path
    return value.rsplit("/", 1)[-1].replace("'", "")


def parse_links(raw: str) -> list[PinLink]:
    if not raw:
        return []
    return [
        PinLink(node_name=node, pin_id=pin_id)
        for node, pin_id in re.findall(r"([A-Za-z0-9_]+)\s+([A-Fa-f0-9]+)", raw)
    ]


def parse_pin(line: str) -> Pin | None:
    open_index = line.find("(")
    close_index = line.rfind(")")
    if open_index < 0 or close_index < 0:
        return None

    raw = line[open_index + 1 : close_index]
    pin_id = field_value(raw, "PinId")
    name = field_value(raw, "PinName")
    direction = "Out" if 'Direction="EGPD_Output"' in raw else "In"
    category = field_value(raw, "PinType.PinCategory")
    sub_category = field_value(raw, "PinType.PinSubCategory")
    object_type = simplify_object_path(field_value(raw, "PinType.PinSubCategoryObject"))
    default_value = field_value(raw, "DefaultValue")
    links = parse_links(field_value(raw, "LinkedTo"))

    if not pin_id:
        return None

    return Pin(
        pin_id=pin_id,
        name=name,
        direction=direction,
        category=category,
        sub_category=sub_category,
        object_type=object_type,
        default_value=default_value,
        links=links,
    )


def parse_nodes(text: str) -> list[Node]:
    lines = text.splitlines()
    nodes: list[Node] = []
    current: list[str] = []
    start_line = 0

    for index, line in enumerate(lines, start=1):
        if line.startswith("Begin Object "):
            current = [line]
            start_line = index
            continue

        if current:
            current.append(line)
            if line.strip() == "End Object":
                body = "\n".join(current)
                header = current[0]
                match = re.search(r"Begin Object Class=(.*?) Name=\"(.*?)\"", header)
                if match:
                    class_path, name = match.groups()
                    class_name = class_path.rsplit(".", 1)[-1].rsplit("/", 1)[-1]
                    node = Node(
                        name=name,
                        class_path=class_path,
                        class_name=class_name,
                        start_line=start_line,
                        end_line=index,
                        body=body,
                    )
                    x = re.search(r"NodePosX=(-?\d+)", body)
                    y = re.search(r"NodePosY=(-?\d+)", body)
                    node.pos_x = int(x.group(1)) if x else None
                    node.pos_y = int(y.group(1)) if y else None
                    fn = field_value(body, "FunctionReference")
                    node.function_parent, node.function_name = parse_member_reference(fn)
                    event = field_value(body, "EventReference")
                    node.event_parent, node.event_name = parse_member_reference(event)
                    node.custom_function_name = field_value(body, "CustomFunctionName")
                    node.delegate_property_name = field_value(body, "DelegatePropertyName")
                    node.delegate_owner_class = field_value(body, "DelegateOwnerClass")
                    node.component_property_name = field_value(body, "ComponentPropertyName")
                    macro = field_value(body, "MacroGraphReference")
                    node.macro_name = simplify_object_path(field_value(macro, "MacroGraph"))
                    node.comment = field_value(body, "NodeComment")
                    for pin_line in (l for l in current if PIN_LINE_PREFIX in l):
                        pin = parse_pin(pin_line)
                        if pin:
                            node.pins.append(pin)
                    nodes.append(node)
                current = []

    return nodes


def node_label(node: Node) -> str:
    if node.class_name == "K2Node_ComponentBoundEvent":
        delegate_name = node.delegate_property_name or node.event_name or node.custom_function_name
        if node.component_property_name and delegate_name:
            base = f"{delegate_name} ({node.component_property_name})"
        else:
            base = delegate_name or "ComponentBoundEvent"
    elif node.class_name == "K2Node_CustomEvent":
        base = node.custom_function_name or node.event_name or "CustomEvent"
    elif node.class_name == "K2Node_Event":
        base = node.event_name or node.custom_function_name or "Event"
    elif node.function_name:
        base = node.function_name
    elif node.macro_name:
        base = f"Macro:{node.macro_name}"
    elif node.comment:
        base = node.comment
    else:
        base = node.class_name.replace("K2Node_", "")
    return f"{base} [{node.name}]"


def event_details(node: Node) -> list[str]:
    details: list[str] = []
    if node.class_name == "K2Node_ComponentBoundEvent":
        if node.delegate_property_name:
            details.append(f"delegate `{node.delegate_property_name}`")
        if node.component_property_name:
            details.append(f"component `{node.component_property_name}`")
        if node.event_name:
            details.append(f"signature `{node.event_name}`")
        if node.custom_function_name:
            details.append(f"binds `{node.custom_function_name}`")
    elif node.class_name in {"K2Node_Event", "K2Node_CustomEvent"}:
        event_name = node.event_name or node.custom_function_name
        if event_name:
            details.append(f"`{event_name}`")
        if node.event_parent:
            details.append(f"parent `{simplify_object_path(node.event_parent)}`")
        if node.custom_function_name and node.custom_function_name != event_name:
            details.append(f"custom `{node.custom_function_name}`")
    return details


def pin_label(pin: Pin) -> str:
    if pin.name:
        return pin.name
    return pin.pin_id[:8]


def sorted_nodes(nodes: Iterable[Node]) -> list[Node]:
    return sorted(nodes, key=lambda n: (n.pos_y if n.pos_y is not None else 0, n.pos_x if n.pos_x is not None else 0, n.start_line))


def build_pin_index(nodes: list[Node]) -> dict[str, tuple[Node, Pin]]:
    index: dict[str, tuple[Node, Pin]] = {}
    for node in nodes:
        for pin in node.pins:
            index[pin.pin_id] = (node, pin)
    return index


def collapse_knot_target(
    link: PinLink,
    nodes_by_name: dict[str, Node],
    pin_index: dict[str, tuple[Node, Pin]],
    category: str,
    seen: set[tuple[str, str]] | None = None,
) -> list[tuple[Node, Pin]]:
    if seen is None:
        seen = set()
    key = (link.node_name, link.pin_id)
    if key in seen:
        return []
    seen.add(key)

    node = nodes_by_name.get(link.node_name)
    target = pin_index.get(link.pin_id)
    if not node or not target:
        return []

    _, pin = target
    if node.class_name == "K2Node_Knot":
        next_pins = [
            p for p in node.pins
            if p.direction == "Out" and (not category or p.category == category)
        ]
        resolved: list[tuple[Node, Pin]] = []
        for next_pin in next_pins:
            for next_link in next_pin.links:
                resolved.extend(collapse_knot_target(next_link, nodes_by_name, pin_index, category, seen))
        return resolved

    return [(node, pin)]


def collect_graph_io(nodes: list[Node]) -> tuple[list[str], list[str]]:
    inputs: list[str] = []
    outputs: list[str] = []
    for node in nodes:
        if node.class_name != "K2Node_Tunnel":
            continue
        out_pins = [pin for pin in node.pins if pin.direction == "Out"]
        in_pins = [pin for pin in node.pins if pin.direction == "In"]
        if out_pins:
            for pin in out_pins:
                default = f" = {pin.default_value}" if pin.default_value else ""
                pin_type = pin.object_type or pin.sub_category or pin.category
                inputs.append(f"- `{pin_label(pin)}` ({pin_type}){default}")
        if in_pins:
            for pin in in_pins:
                pin_type = pin.object_type or pin.sub_category or pin.category
                outputs.append(f"- `{pin_label(pin)}` ({pin_type})")
    return inputs, outputs


def collect_entry_nodes(nodes: list[Node]) -> list[str]:
    rows: list[str] = []
    entry_classes = {
        "K2Node_FunctionEntry": "函数入口",
        "K2Node_Event": "事件入口",
        "K2Node_CustomEvent": "自定义事件",
        "K2Node_ComponentBoundEvent": "组件绑定事件",
    }
    for node in sorted_nodes(n for n in nodes if n.class_name in entry_classes):
        details: list[str] = []
        if node.class_name == "K2Node_FunctionEntry":
            if node.function_name:
                details.append(f"`{node.function_name}`")
            if node.function_parent:
                details.append(f"parent `{simplify_object_path(node.function_parent)}`")
        else:
            details.extend(event_details(node))
        suffix = f": {'; '.join(details)}" if details else ""
        rows.append(f"- {entry_classes[node.class_name]} {node.name}{suffix}")
    return rows


def collect_exec_edges(nodes: list[Node]) -> list[str]:
    nodes_by_name = {node.name: node for node in nodes}
    pin_index = build_pin_index(nodes)
    edges: list[str] = []

    for node in sorted_nodes(nodes):
        if node.class_name == "K2Node_Knot":
            continue
        for pin in node.pins:
            if pin.direction != "Out" or pin.category != "exec" or not pin.links:
                continue
            targets: list[str] = []
            for link in pin.links:
                for target_node, target_pin in collapse_knot_target(link, nodes_by_name, pin_index, "exec"):
                    targets.append(f"{node_label(target_node)}.`{pin_label(target_pin)}`")
            if targets:
                edges.append(f"- {node_label(node)}.`{pin_label(pin)}` -> {', '.join(targets)}")
    return edges


def collect_variables(nodes: list[Node]) -> list[str]:
    rows: list[str] = []
    for node in sorted_nodes(n for n in nodes if n.class_name == "K2Node_TemporaryVariable"):
        pin = next((p for p in node.pins if p.name == "Variable"), None)
        var_type = pin.sub_category or pin.category if pin else ""
        comment = node.comment or node.name
        rows.append(f"- `{comment}` ({var_type}) [{node.name}]")
    return rows


def collect_function_nodes(nodes: list[Node]) -> list[str]:
    rows: list[str] = []
    for node in sorted_nodes(
        n for n in nodes
        if n.function_name
        or n.event_name
        or n.custom_function_name
        or n.delegate_property_name
        or n.macro_name
        or n.comment
    ):
        details: list[str] = []
        if node.function_name:
            details.append(f"`{node.function_name}`")
        if node.function_parent:
            details.append(f"parent `{simplify_object_path(node.function_parent)}`")
        details.extend(event_details(node))
        if node.macro_name:
            details.append(f"`{node.macro_name}`")
        if node.comment:
            details.append(f"注释: {node.comment}")
        if details:
            rows.append(f"- {node.name}: " + "; ".join(details))
    return rows


def collect_data_edges(nodes: list[Node], limit: int) -> list[str]:
    nodes_by_name = {node.name: node for node in nodes}
    pin_index = build_pin_index(nodes)
    rows: list[str] = []

    for node in sorted_nodes(nodes):
        if node.class_name == "K2Node_Knot":
            continue
        for pin in node.pins:
            if pin.direction != "Out" or pin.category == "exec" or not pin.links:
                continue
            targets: list[str] = []
            for link in pin.links:
                for target_node, target_pin in collapse_knot_target(link, nodes_by_name, pin_index, pin.category):
                    targets.append(f"{node_label(target_node)}.`{pin_label(target_pin)}`")
            if targets:
                rows.append(f"- {node_label(node)}.`{pin_label(pin)}` -> {', '.join(targets)}")
            if len(rows) >= limit:
                rows.append(f"- ... 已截断，仅显示前 {limit} 条数据连接")
                return rows
    return rows


def to_markdown(source: Path, nodes: list[Node], data_edge_limit: int) -> str:
    tunnel_inputs, tunnel_outputs = collect_graph_io(nodes)
    entry_nodes = collect_entry_nodes(nodes)
    exec_edges = collect_exec_edges(nodes)
    variables = collect_variables(nodes)
    function_nodes = collect_function_nodes(nodes)
    data_edges = collect_data_edges(nodes, data_edge_limit)

    lines: list[str] = [
        f"# UE Blueprint 导出逻辑流",
        "",
        f"- 源文件: `{source}`",
        f"- 节点数: {len(nodes)}",
        f"- 函数/宏/注释节点: {len(function_nodes)}",
        "",
        "## 入口节点",
        *(entry_nodes or ["- 未发现事件、函数入口或组件绑定入口"]),
        "",
        "## Tunnel 输入",
        *(tunnel_inputs or ["- 无（EventGraph/ConstructionScript 通常不使用 Tunnel 输入）"]),
        "",
        "## Tunnel 输出",
        *(tunnel_outputs or ["- 无（EventGraph/ConstructionScript 通常不使用 Tunnel 输出）"]),
        "",
        "## 临时变量",
        *(variables or ["- 无"]),
        "",
        "## 执行流",
        *(exec_edges or ["- 未发现执行线"]),
        "",
        "## 关键函数与注释节点",
        *(function_nodes or ["- 无"]),
        "",
        "## 数据连接摘要",
        *(data_edges or ["- 无"]),
        "",
    ]
    return "\n".join(lines)


def to_json(source: Path, nodes: list[Node]) -> str:
    payload = {
        "source": str(source),
        "nodes": [
            {
                "name": node.name,
                "class": node.class_name,
                "line": node.start_line,
                "pos": [node.pos_x, node.pos_y],
                "function": node.function_name,
                "function_parent": node.function_parent,
                "event": node.event_name,
                "event_parent": node.event_parent,
                "custom_function": node.custom_function_name,
                "delegate_property": node.delegate_property_name,
                "delegate_owner_class": node.delegate_owner_class,
                "component_property": node.component_property_name,
                "macro": node.macro_name,
                "comment": node.comment,
                "pins": [
                    {
                        "id": pin.pin_id,
                        "name": pin.name,
                        "direction": pin.direction,
                        "category": pin.category,
                        "sub_category": pin.sub_category,
                        "object_type": pin.object_type,
                        "default_value": pin.default_value,
                        "links": [{"node": link.node_name, "pin": link.pin_id} for link in pin.links],
                    }
                    for pin in node.pins
                ],
            }
            for node in nodes
        ],
    }
    return json.dumps(payload, ensure_ascii=False, indent=2)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze Unreal Engine Blueprint copied/exported text.")
    parser.add_argument("input", type=Path, help="UE Blueprint copy/export text file")
    parser.add_argument("-o", "--output", type=Path, help="Output file. Defaults to stdout.")
    parser.add_argument("--format", choices=("markdown", "json"), default="markdown", help="Output format")
    parser.add_argument("--data-edge-limit", type=int, default=80, help="Maximum data edges in markdown output")
    return parser.parse_args()


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8")

    args = parse_args()
    if not args.input.exists():
        print(f"输入文件不存在: {args.input}")
        return 1

    text = read_text_auto(args.input)
    nodes = parse_nodes(text)
    if args.format == "json":
        output = to_json(args.input, nodes)
    else:
        output = to_markdown(args.input, nodes, args.data_edge_limit)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
        print(f"已生成: {args.output}")
    else:
        print(output)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
