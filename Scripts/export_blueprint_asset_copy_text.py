import argparse
import json
import re
import sys
import tempfile
from collections import Counter, defaultdict
from pathlib import Path

import unreal


def parse_args():
    parser = argparse.ArgumentParser(
        description="Export loaded Unreal Blueprint graph nodes to UE copy-text files."
    )
    parser.add_argument("--asset", required=True, help="Blueprint asset object path, e.g. /Game/Foo/BP.BP")
    parser.add_argument("--output-dir", required=True, help="Directory for exported graph text files")
    return parser.parse_args()


def sanitize_filename(name):
    value = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", name)
    return value.strip(" .") or "Graph"


def read_exported_text(path):
    data = path.read_bytes()
    if data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
        return data.decode("utf-16")
    if data[:256].count(b"\x00") > 8:
        return data.decode("utf-16")
    for encoding in ("utf-8-sig", "utf-8", "mbcs"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", errors="replace")


def is_graph_object(obj, asset_path):
    try:
        return obj.get_class().get_name() == "EdGraph" and obj.get_path_name().startswith(asset_path + ":")
    except Exception:
        return False


def is_node_object(obj, graph_path):
    try:
        outer = obj.get_outer()
        if not outer or outer.get_path_name() != graph_path:
            return False
        class_name = obj.get_class().get_name()
        return class_name.startswith("K2Node_") or class_name == "EdGraphNode_Comment"
    except Exception:
        return False


def export_node_to_text(node, temp_dir):
    safe_name = sanitize_filename(node.get_name())
    temp_path = Path(temp_dir) / (safe_name + ".copy")
    task = unreal.AssetExportTask()
    task.object = node
    task.filename = str(temp_path)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.ObjectExporterT3D()
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError("Export failed: {}".format(node.get_path_name()))
    return read_exported_text(temp_path).strip()


def main():
    args = parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    asset = unreal.EditorAssetLibrary.load_asset(args.asset)
    if not asset:
        raise RuntimeError("Blueprint asset not loaded: {}".format(args.asset))

    graphs = []
    for obj in unreal.ObjectIterator():
        if is_graph_object(obj, args.asset):
            graphs.append(obj)

    graphs.sort(key=lambda graph: graph.get_path_name())

    nodes_by_graph = defaultdict(list)
    for graph in graphs:
        graph_path = graph.get_path_name()
        for obj in unreal.ObjectIterator():
            if is_node_object(obj, graph_path):
                nodes_by_graph[graph_path].append(obj)

    manifest = {
        "asset": args.asset,
        "asset_class": asset.get_class().get_name(),
        "graphs": [],
    }

    with tempfile.TemporaryDirectory() as temp_dir:
        for graph in graphs:
            graph_path = graph.get_path_name()
            nodes = nodes_by_graph.get(graph_path, [])
            nodes.sort(key=lambda node: node.get_name())
            chunks = []
            errors = []

            for node in nodes:
                try:
                    chunks.append(export_node_to_text(node, temp_dir))
                except Exception as exc:
                    errors.append({"node": node.get_path_name(), "error": str(exc)})

            text = "\n".join(chunk for chunk in chunks if chunk)
            graph_file = output_dir / (sanitize_filename(graph.get_name()) + ".md")
            graph_file.write_text(text + ("\n" if text else ""), encoding="utf-8")

            class_counts = Counter(node.get_class().get_name() for node in nodes)
            manifest["graphs"].append(
                {
                    "name": graph.get_name(),
                    "path": graph_path,
                    "file": str(graph_file),
                    "node_count": len(nodes),
                    "exported_node_count": len(chunks),
                    "errors": errors,
                    "class_counts": dict(sorted(class_counts.items())),
                }
            )

    manifest_path = output_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("Exported Blueprint copy text manifest: {}".format(manifest_path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
