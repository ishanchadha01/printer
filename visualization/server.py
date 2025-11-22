"""
Simple Flask server that wraps visualize_path.py so the React client can request
a rendered Matplotlib preview. Run with:

    python visualization/server.py --module-path /path/to/build

The endpoint expects form-data with an STL file under "stl" plus the optional
parameters documented below, and returns a JSON payload containing a data URL
for the PNG image and a few metadata fields.
"""

import argparse
import base64
import tempfile
from io import BytesIO
from pathlib import Path
from typing import Optional

import matplotlib

matplotlib.use("Agg")  # Use a non-GUI backend for server-side rendering.

from flask import Flask, jsonify, request
from matplotlib import cm, pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

from visualize_path import (
    add_module_path,
    plot_layer_paths,
    plot_mesh,
    plot_raw_intersections,
    triangles_intersecting_layer,
)


def plot_triangles(ax, mesh, tris, cmap):
    """Light wrapper so we can color intersecting triangles for the chosen layer."""
    for order_idx, (tri_idx, tri) in enumerate(tris):
        pts = mesh.points
        verts = [
            (pts[tri.vertices[0]].x, pts[tri.vertices[0]].y, pts[tri.vertices[0]].z),
            (pts[tri.vertices[1]].x, pts[tri.vertices[1]].y, pts[tri.vertices[1]].z),
            (pts[tri.vertices[2]].x, pts[tri.vertices[2]].y, pts[tri.vertices[2]].z),
        ]
        color = cmap(order_idx / max(1, len(tris)))
        collection = Poly3DCollection([verts], alpha=0.45, facecolor=color, edgecolor="k", linewidths=0.6, label=f"tri {tri_idx}")
        ax.add_collection3d(collection)


def render_visualization(
    stl_path: Path,
    module_path: Optional[Path] = None,
    layer_height: int = 1,
    infill_spacing: float = 1.0,
    layer_idx: int = 0,
    show_mesh: bool = True,
    show_contours: bool = True,
    show_infill: bool = True,
    show_raw_intersections: bool = False,
    color_intersecting_tris: bool = False,
) -> dict:
    add_module_path(module_path)

    try:
        import pathplan_bindings as pp
    except ImportError as exc:  # pragma: no cover - depends on local build
        raise RuntimeError(f"Failed to import pathplan_bindings: {exc}")

    planner = pp.PathPlanner()
    planner.set_cad(str(stl_path))
    planner.slice_planar(layer_height, infill_spacing)

    if planner.layer_count() == 0:
        raise RuntimeError("No layers generated; check STL or slicing parameters.")

    layer_idx = max(0, min(layer_idx, planner.layer_count() - 1))
    layer = planner.get_layer(layer_idx)
    raw_layers = planner.get_raw_layers()
    raw_idx = int(round(layer.z / max(1, layer_height)))
    raw_pts = raw_layers[raw_idx] if raw_idx < len(raw_layers) else []

    fig = plt.figure(figsize=(8, 6))
    ax = fig.add_subplot(111, projection="3d")

    if show_mesh:
        plot_mesh(ax, planner.get_meshes())
    plot_layer_paths(ax, layer, show_contours=show_contours, show_infill=show_infill)
    if show_raw_intersections:
        plot_raw_intersections(ax, raw_pts, layer.z)

    if color_intersecting_tris:
        cmap = cm.get_cmap("tab20")
        for mesh in planner.get_meshes():
            tris = triangles_intersecting_layer(mesh, layer.z)
            plot_triangles(ax, mesh, tris, cmap)

    ax.set_title(f"Layer {layer_idx} at z={layer.z}mm")
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    ax.view_init(elev=30, azim=-60)
    plt.tight_layout()

    buf = BytesIO()
    fig.savefig(buf, format="png", facecolor="#0b1021")
    plt.close(fig)
    buf.seek(0)
    encoded = base64.b64encode(buf.read()).decode("ascii")
    return {
        "image": f"data:image/png;base64,{encoded}",
        "meta": {"layers": planner.layer_count(), "selectedLayer": layer_idx, "zHeight": layer.z},
    }


def build_app(module_path: Optional[Path]):
    module_path = Path(module_path) if module_path else None
    app = Flask(__name__)

    @app.after_request
    def add_cors_headers(response):
        response.headers["Access-Control-Allow-Origin"] = "*"
        response.headers["Access-Control-Allow-Headers"] = "Content-Type"
        response.headers["Access-Control-Allow-Methods"] = "POST, OPTIONS"
        return response

    @app.route("/visualize", methods=["POST", "OPTIONS"])
    def visualize():
        if request.method == "OPTIONS":
            return ("", 204)

        upload = request.files.get("stl")
        if not upload:
            return jsonify({"error": "Missing STL upload under `stl` field"}), 400

        params = request.form
        try:
            layer_height = int(params.get("layerHeight", 1))
            infill_spacing = float(params.get("infillSpacing", 1.0))
            layer_idx = int(params.get("layer", 0))
            show_mesh = params.get("showMesh", "true") == "true"
            show_contours = params.get("showContours", "true") == "true"
            show_infill = params.get("showInfill", "true") == "true"
            show_raw = params.get("showRawIntersections", "false") == "true"
            color_tris = params.get("colorIntersections", "false") == "true"
        except ValueError:
            return jsonify({"error": "Invalid numeric parameter."}), 400

        with tempfile.NamedTemporaryFile(delete=False, suffix=".stl") as tmp:
            upload.save(tmp.name)
            stl_path = Path(tmp.name)

        try:
            result = render_visualization(
                stl_path=stl_path,
                module_path=module_path,
                layer_height=layer_height,
                infill_spacing=infill_spacing,
                layer_idx=layer_idx,
                show_mesh=show_mesh,
                show_contours=show_contours,
                show_infill=show_infill,
                show_raw_intersections=show_raw,
                color_intersecting_tris=color_tris,
            )
        except Exception as exc:  # pragma: no cover - surfaced to client
            return jsonify({"error": str(exc)}), 500
        finally:
            stl_path.unlink(missing_ok=True)

        return jsonify(result)

    return app


def main():
    parser = argparse.ArgumentParser(description="Serve a simple visualization API for the React client.")
    parser.add_argument("--module-path", type=Path, default=None, help="Optional path to built pathplan_bindings module (e.g., build directory).")
    parser.add_argument("--port", type=int, default=8000, help="Port to listen on.")
    args = parser.parse_args()

    app = build_app(args.module_path)
    app.run(host="0.0.0.0", port=args.port, debug=False)


if __name__ == "__main__":
    main()
