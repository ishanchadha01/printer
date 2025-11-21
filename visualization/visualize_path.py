import argparse
import sys
from pathlib import Path
from typing import Iterable, Tuple

import matplotlib.pyplot as plt
from matplotlib import cm
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def add_module_path(extra_path: Path):
    if extra_path and extra_path.exists():
        sys.path.insert(0, str(extra_path))


def parse_args():
    parser = argparse.ArgumentParser(description="Visualize STL and planar slice path using PathPlanner bindings.")
    parser.add_argument("stl", type=Path, help="Path to STL file.")
    parser.add_argument("--layer-height", type=int, default=1, help="Layer height in mm.")
    parser.add_argument("--infill-spacing", type=float, default=1.0, help="Grid infill spacing.")
    parser.add_argument("--layer", type=int, default=0, help="Layer index to visualize from the sliced plan.")
    parser.add_argument("--module-path", type=Path, default=None, help="Optional path to built pathplan_bindings module (e.g., build directory).")
    parser.add_argument("--show-mesh", action="store_true", help="Display STL mesh.")
    parser.add_argument("--show-contours", action="store_true", help="Display contour segments.")
    parser.add_argument("--show-infill", action="store_true", help="Display infill lines.")
    parser.add_argument("--show-raw-intersections", action="store_true", help="Display raw intersection points from populate_layer_lists.")
    parser.add_argument("--color-intersecting-tris", action="store_true", help="Color triangles intersecting selected layer.")
    parser.add_argument("--list-intersecting-tris", action="store_true", help="Print triangle indices and vertices intersecting the layer.")
    return parser.parse_args()


def plot_mesh(ax, meshes):
    for mesh in meshes:
        faces = []
        for tri in mesh.triangles:
            pts = mesh.points
            faces.append(
                [
                    (pts[tri.vertices[0]].x, pts[tri.vertices[0]].y, pts[tri.vertices[0]].z),
                    (pts[tri.vertices[1]].x, pts[tri.vertices[1]].y, pts[tri.vertices[1]].z),
                    (pts[tri.vertices[2]].x, pts[tri.vertices[2]].y, pts[tri.vertices[2]].z),
                ]
            )
        if faces:
            collection = Poly3DCollection(faces, alpha=0.2, facecolor="gray", edgecolor="black", linewidths=0.3)
            ax.add_collection3d(collection)


def plot_layer_paths(ax, layer, show_contours: bool, show_infill: bool):
    if show_contours:
        for start, end in layer.contours:
            ax.plot([start.x, end.x], [start.y, end.y], [start.z, end.z], color="tab:blue", linewidth=2, label="contour")
    if show_infill:
        for start, end in layer.infill:
            ax.plot([start.x, end.x], [start.y, end.y], [start.z, end.z], color="tab:orange", linewidth=1, linestyle="--", label="infill")


def plot_raw_intersections(ax, raw_pts: Iterable, z: float):
    if not raw_pts:
        return
    xs, ys = zip(*[(p.x, p.y) for p in raw_pts])
    zs = [z for _ in raw_pts]
    ax.scatter(xs, ys, zs, color="red", s=8, alpha=0.8, label="raw intersections")


def triangles_intersecting_layer(mesh, z: float, tol: float = 1e-5) -> Iterable[Tuple[int, object]]:
    hits = []
    for idx, tri in enumerate(mesh.triangles):
        zs = [mesh.points[tri.vertices[i]].z for i in range(3)]
        if min(zs) - tol <= z <= max(zs) + tol:
            hits.append((idx, tri))
    return hits


def plot_triangles(ax, mesh, tris, cmap):
    for order_idx, (tri_idx, tri) in enumerate(tris):
        pts = mesh.points
        verts = [
            (pts[tri.vertices[0]].x, pts[tri.vertices[0]].y, pts[tri.vertices[0]].z),
            (pts[tri.vertices[1]].x, pts[tri.vertices[1]].y, pts[tri.vertices[1]].z),
            (pts[tri.vertices[2]].x, pts[tri.vertices[2]].y, pts[tri.vertices[2]].z),
        ]
        color = cmap(order_idx / max(1, len(tris)))
        collection = Poly3DCollection([verts], alpha=0.5, facecolor=color, edgecolor="k", linewidths=0.6, label=f"tri {tri_idx}")
        ax.add_collection3d(collection)


def main():
    args = parse_args()
    add_module_path(args.module_path)

    try:
        import pathplan_bindings as pp
    except ImportError as exc:
        sys.exit(f"Failed to import pathplan_bindings: {exc}")

    planner = pp.PathPlanner()
    planner.set_cad(str(args.stl))
    planner.slice_planar(args.layer_height, args.infill_spacing)

    if planner.layer_count() == 0:
        sys.exit("No layers generated; check STL or slicing parameters.")

    layer_idx = max(0, min(args.layer, planner.layer_count() - 1))
    layer = planner.get_layer(layer_idx)
    raw_layers = planner.get_raw_layers()
    raw_idx = int(round(layer.z / max(1, args.layer_height)))
    raw_pts = raw_layers[raw_idx] if raw_idx < len(raw_layers) else []

    fig = plt.figure(figsize=(8, 6))
    ax = fig.add_subplot(111, projection="3d")

    if args.show_mesh:
        plot_mesh(ax, planner.get_meshes())
    plot_layer_paths(ax, layer, show_contours=args.show_contours, show_infill=args.show_infill)
    if args.show_raw_intersections:
        plot_raw_intersections(ax, raw_pts, layer.z)

    if args.color_intersecting_tris:
        cmap = cm.get_cmap("tab20")
        for mesh in planner.get_meshes():
            tris = triangles_intersecting_layer(mesh, layer.z)
            plot_triangles(ax, mesh, tris, cmap)
            if args.list_intersecting_tris and tris:
                print(f"Layer z={layer.z}: {len(tris)} intersecting tris")
                for idx, tri in tris:
                    pts = mesh.points
                    vertices = [pts[tri.vertices[i]] for i in range(3)]
                    print(f"  tri {idx}: {[(v.x, v.y, v.z) for v in vertices]}")

    ax.set_title(f"Layer {layer_idx} at z={layer.z}mm")
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    ax.view_init(elev=30, azim=-60)

    # Prevent duplicate labels when plotting multiple segments
    handles, labels = ax.get_legend_handles_labels()
    unique = dict(zip(labels, handles))
    if unique:
        ax.legend(unique.values(), unique.keys())

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
