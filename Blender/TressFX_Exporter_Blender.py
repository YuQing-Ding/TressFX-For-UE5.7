bl_info = {
    "name": "TressFX Exporter",
    "author": "OpenAI Codex & YuQing-Ding",
    "version": (0, 1, 0),
    "blender": (3, 3, 0),
    "location": "File > Export > TressFX (.tfx)",
    "description": "Export Blender curve strands to AMD TressFX .tfx and optional .tfxbone files",
    "category": "Import-Export",
}

import bisect
import os
import struct

import bpy
from bpy.props import (
    BoolProperty,
    EnumProperty,
    FloatProperty,
    IntProperty,
    StringProperty,
)
from bpy_extras.io_utils import ExportHelper
from mathutils import Vector
from mathutils.bvhtree import BVHTree


TFX_VERSION = 4.0
TFX_HEADER_FORMAT = "<f7I32I"
TFX_HEADER_SIZE = struct.calcsize(TFX_HEADER_FORMAT)


def spline_points_to_world(obj, spline):
    points = []

    if spline.type == "BEZIER":
        source_points = spline.bezier_points
        for point in source_points:
            points.append(obj.matrix_world @ point.co)
    else:
        source_points = spline.points
        for point in source_points:
            co = point.co
            points.append(obj.matrix_world @ Vector((co.x, co.y, co.z)))

    if spline.use_cyclic_u and len(points) > 1:
        points.append(points[0].copy())

    return points


def extract_legacy_curve_strands(obj):
    strands = []

    for spline in obj.data.splines:
        points = spline_points_to_world(obj, spline)
        if len(points) >= 2:
            strands.append(points)

    return strands


def _read_curve_offsets(curves_data):
    offsets = None

    raw_offsets = getattr(curves_data, "curve_offset_data", None)
    if raw_offsets:
        offsets = []
        for item in raw_offsets:
            if hasattr(item, "value"):
                offsets.append(int(item.value))
            else:
                offsets.append(int(item))

    if offsets and len(offsets) >= 2:
        return offsets

    raw_offsets = getattr(curves_data, "curve_offsets", None)
    if raw_offsets:
        offsets = []
        for item in raw_offsets:
            if hasattr(item, "value"):
                offsets.append(int(item.value))
            else:
                offsets.append(int(item))

    if offsets and len(offsets) >= 2:
        return offsets

    return None


def _get_attribute_vector_data(curves_data, name):
    attributes = getattr(curves_data, "attributes", None)
    if attributes is None:
        return None

    attribute = attributes.get(name)
    if attribute is None:
        return None

    vectors = []
    for item in attribute.data:
        if hasattr(item, "vector"):
            vectors.append(Vector(item.vector))
        elif hasattr(item, "co"):
            vectors.append(Vector(item.co))
        elif hasattr(item, "value"):
            vectors.append(Vector(item.value))
        else:
            return None

    return vectors


def extract_new_curves_strands(obj, depsgraph):
    eval_obj = obj.evaluated_get(depsgraph)
    curves_data = eval_obj.data
    strands = []

    # Blender's Curves API has shifted a bit across 3.x/4.x. Prefer the point
    # position attribute plus curve offsets because it is closest to storage.
    point_positions = _get_attribute_vector_data(curves_data, "position")
    offsets = _read_curve_offsets(curves_data)

    if point_positions and offsets:
        for start, end in zip(offsets[:-1], offsets[1:]):
            points = [eval_obj.matrix_world @ point_positions[index] for index in range(start, end)]
            if len(points) >= 2:
                strands.append(points)
        return strands

    # Fallback for APIs exposing each curve as an object with a .points slice.
    curve_items = getattr(curves_data, "curves", None)
    if curve_items:
        for curve_item in curve_items:
            raw_points = getattr(curve_item, "points", None)
            if not raw_points:
                continue

            points = []
            for point in raw_points:
                if hasattr(point, "position"):
                    co = Vector(point.position)
                elif hasattr(point, "co"):
                    co = Vector(point.co)
                elif hasattr(point, "index") and point_positions:
                    co = point_positions[point.index]
                else:
                    points = []
                    break
                points.append(eval_obj.matrix_world @ co)

            if len(points) >= 2:
                strands.append(points)

    return strands


def extract_object_strands(obj, depsgraph):
    if obj.type == "CURVE":
        return extract_legacy_curve_strands(obj)
    if obj.type == "CURVES":
        return extract_new_curves_strands(obj, depsgraph)
    return []


def resample_polyline(points, target_count):
    if len(points) < 2:
        return []

    lengths = [0.0]
    total = 0.0
    for previous, current in zip(points[:-1], points[1:]):
        total += (current - previous).length
        lengths.append(total)

    if total <= 1.0e-8:
        return [points[0].copy() for _ in range(target_count)]

    resampled = []
    for index in range(target_count):
        distance = total * (index / (target_count - 1))
        upper = bisect.bisect_left(lengths, distance)

        if upper <= 0:
            resampled.append(points[0].copy())
            continue
        if upper >= len(points):
            resampled.append(points[-1].copy())
            continue

        lower_distance = lengths[upper - 1]
        upper_distance = lengths[upper]
        span = upper_distance - lower_distance
        alpha = 0.0 if span <= 1.0e-8 else (distance - lower_distance) / span
        resampled.append(points[upper - 1].lerp(points[upper], alpha))

    return resampled


def barycentric_coordinates(point, a, b, c):
    v0 = b - a
    v1 = c - a
    v2 = point - a
    d00 = v0.dot(v0)
    d01 = v0.dot(v1)
    d11 = v1.dot(v1)
    d20 = v2.dot(v0)
    d21 = v2.dot(v1)
    denom = d00 * d11 - d01 * d01

    if abs(denom) <= 1.0e-12:
        return (1.0, 0.0, 0.0)

    v = (d11 * d20 - d01 * d21) / denom
    w = (d00 * d21 - d01 * d20) / denom
    u = 1.0 - v - w
    return (u, v, w)


def clamp_barycentric(weights):
    clamped = [max(0.0, value) for value in weights]
    total = sum(clamped)
    if total <= 1.0e-8:
        return (1.0, 0.0, 0.0)
    return tuple(value / total for value in clamped)


class MeshBindData:
    def __init__(self, context, mesh_obj, armature_obj, invert_uv_y, max_influences, normalize_weights):
        self.context = context
        self.mesh_obj = mesh_obj
        self.invert_uv_y = invert_uv_y
        self.max_influences = max_influences
        self.normalize_weights = normalize_weights

        depsgraph = context.evaluated_depsgraph_get()
        self.eval_obj = mesh_obj.evaluated_get(depsgraph)
        self.mesh = self.eval_obj.to_mesh()
        self.mesh.calc_loop_triangles()

        self.vertices = [self.eval_obj.matrix_world @ vertex.co for vertex in self.mesh.vertices]
        self.triangles = [tuple(loop_tri.vertices) for loop_tri in self.mesh.loop_triangles]
        self.loop_triangles = list(self.mesh.loop_triangles)
        self.uv_layer = self.mesh.uv_layers.active

        if self.triangles:
            self.bvh = BVHTree.FromPolygons(self.vertices, self.triangles, all_triangles=True)
        else:
            self.bvh = None

        self.bone_names, self.group_to_bone_index = self._build_bone_map(armature_obj)

    def cleanup(self):
        if self.eval_obj is not None:
            self.eval_obj.to_mesh_clear()

    def _build_bone_map(self, armature_obj):
        vertex_groups = self.mesh_obj.vertex_groups
        group_to_bone_index = {}
        bone_names = []

        if armature_obj is not None:
            group_name_to_index = {group.name: group.index for group in vertex_groups}
            for bone in armature_obj.data.bones:
                group_index = group_name_to_index.get(bone.name)
                if group_index is None:
                    continue
                group_to_bone_index[group_index] = len(bone_names)
                bone_names.append(bone.name)
        else:
            for group in vertex_groups:
                group_to_bone_index[group.index] = len(bone_names)
                bone_names.append(group.name)

        return bone_names, group_to_bone_index

    def _triangle_uv(self, loop_triangle, barycentric):
        if self.uv_layer is None:
            return (0.0, 0.0)

        uv = Vector((0.0, 0.0))
        for loop_index, weight in zip(loop_triangle.loops, barycentric):
            uv += self.uv_layer.data[loop_index].uv * weight

        if self.invert_uv_y:
            uv.y = 1.0 - uv.y

        return (uv.x, uv.y)

    def _triangle_weights(self, loop_triangle, barycentric):
        accumulated = {}

        for vertex_index, bary_weight in zip(loop_triangle.vertices, barycentric):
            if bary_weight <= 0.0:
                continue

            for group in self.mesh.vertices[vertex_index].groups:
                bone_index = self.group_to_bone_index.get(group.group)
                if bone_index is None:
                    continue
                accumulated[bone_index] = accumulated.get(bone_index, 0.0) + group.weight * bary_weight

        influences = [
            (bone_index, weight)
            for bone_index, weight in accumulated.items()
            if weight > 1.0e-6
        ]
        influences.sort(key=lambda item: item[1], reverse=True)
        influences = influences[: self.max_influences]

        if self.normalize_weights:
            total = sum(weight for _bone_index, weight in influences)
            if total > 1.0e-8:
                influences = [(bone_index, weight / total) for bone_index, weight in influences]

        return influences

    def bind_root(self, root_world):
        if self.bvh is None:
            return (0.0, 0.0), []

        location, _normal, tri_index, _distance = self.bvh.find_nearest(root_world)
        if tri_index is None or tri_index < 0:
            return (0.0, 0.0), []

        loop_triangle = self.loop_triangles[tri_index]
        tri_vertices = [self.vertices[index] for index in loop_triangle.vertices]
        barycentric = barycentric_coordinates(location, tri_vertices[0], tri_vertices[1], tri_vertices[2])
        barycentric = clamp_barycentric(barycentric)

        return self._triangle_uv(loop_triangle, barycentric), self._triangle_weights(loop_triangle, barycentric)


def convert_position(position, scale, swap_yz, flip_y, flip_z):
    x = position.x
    y = position.y
    z = position.z

    if swap_yz:
        y, z = z, y
    if flip_y:
        y = -y
    if flip_z:
        z = -z

    return (x * scale, y * scale, z * scale)


def write_tfx(filepath, strands, strand_uvs, num_vertices_per_strand, scale, swap_yz, flip_y, flip_z, lock_tip):
    vertex_position_offset = TFX_HEADER_SIZE
    vertex_position_size = len(strands) * num_vertices_per_strand * struct.calcsize("<4f")
    strand_uv_offset = vertex_position_offset + vertex_position_size

    header = struct.pack(
        TFX_HEADER_FORMAT,
        TFX_VERSION,
        len(strands),
        num_vertices_per_strand,
        vertex_position_offset,
        strand_uv_offset,
        0,
        0,
        0,
        *([0] * 32),
    )

    with open(filepath, "wb") as output:
        output.write(header)

        for strand in strands:
            for vertex_index, position in enumerate(strand):
                x, y, z = convert_position(position, scale, swap_yz, flip_y, flip_z)
                inverse_mass = 0.0 if vertex_index < 2 else 1.0
                if lock_tip and vertex_index == len(strand) - 1:
                    inverse_mass = 0.0
                output.write(struct.pack("<4f", x, y, z, inverse_mass))

        for uv in strand_uvs:
            output.write(struct.pack("<2f", uv[0], uv[1]))


def write_tfxbone(filepath, bone_names, strand_bindings):
    with open(filepath, "wb") as output:
        output.write(struct.pack("<i", len(bone_names)))

        for bone_index, bone_name in enumerate(bone_names):
            encoded_name = bone_name.encode("utf-8")
            output.write(struct.pack("<ii", bone_index, len(encoded_name) + 1))
            output.write(encoded_name)
            output.write(b"\0")

        output.write(struct.pack("<i", len(strand_bindings)))
        for strand_index, influences in enumerate(strand_bindings):
            output.write(struct.pack("<ii", strand_index, len(influences)))
            for bone_index, weight in influences:
                output.write(struct.pack("<if", bone_index, weight))


def output_path_with_extension(filepath, extension):
    root, _ext = os.path.splitext(filepath)
    return root + extension


class TRESSFX_OT_export(bpy.types.Operator, ExportHelper):
    bl_idname = "export_scene.tressfx"
    bl_label = "Export TressFX"
    bl_options = {"PRESET", "UNDO"}

    filename_ext = ".tfx"
    filter_glob: StringProperty(default="*.tfx", options={"HIDDEN"})

    num_vertices_per_strand: EnumProperty(
        name="Vertices Per Strand",
        description="TressFX requires this value to divide 64",
        items=[
            ("4", "4", ""),
            ("8", "8", ""),
            ("16", "16", ""),
            ("32", "32", ""),
            ("64", "64", ""),
        ],
        default="16",
    )

    coordinate_space: EnumProperty(
        name="Space",
        items=[
            ("WORLD", "World", "Export strand positions in Blender world space"),
            ("MESH_LOCAL", "Mesh Local", "Export strand positions relative to the mesh object"),
            ("CURVE_LOCAL", "Curve Local", "Export each strand relative to its curve object"),
        ],
        default="MESH_LOCAL",
    )

    scale: FloatProperty(
        name="Scale",
        default=100.0,
        min=0.0001,
        soft_min=0.01,
        soft_max=1000.0,
    )

    swap_yz: BoolProperty(
        name="Swap Y/Z",
        description="Match the old Maya exporter option for Z-up conversions",
        default=False,
    )

    flip_y: BoolProperty(
        name="Flip Y",
        description="Useful when matching Blender data to UE-imported assets",
        default=False,
    )

    flip_z: BoolProperty(
        name="Flip Z",
        description="Match the old Maya exporter invert-Z option",
        default=False,
    )

    lock_tip: BoolProperty(
        name="Lock Tip",
        description="Write inverse mass 0 for the last vertex of each strand",
        default=False,
    )

    curve_object_name: StringProperty(
        name="Curve Object",
        description="Optional single curve object; if empty, all selected curve objects are exported",
        default="",
    )

    mesh_object_name: StringProperty(
        name="Mesh Object",
        description="Optional mesh used to compute strand root UVs and bone bindings",
        default="",
    )

    armature_object_name: StringProperty(
        name="Armature Object",
        description="Optional armature used to choose exported bone names",
        default="",
    )

    export_tfxbone: BoolProperty(
        name="Export .tfxbone",
        description="Export root skin bindings using mesh vertex groups",
        default=False,
    )

    invert_uv_y: BoolProperty(
        name="Invert UV Y",
        description="Use 1 - V for strand root UVs",
        default=True,
    )

    max_influences: IntProperty(
        name="Max Influences",
        default=16,
        min=1,
        max=16,
    )

    normalize_bone_weights: BoolProperty(
        name="Normalize Weights",
        default=True,
    )

    def draw(self, _context):
        layout = self.layout

        layout.prop_search(self, "curve_object_name", bpy.data, "objects", text="Curve Object")
        layout.prop_search(self, "mesh_object_name", bpy.data, "objects", text="Mesh Object")
        layout.prop_search(self, "armature_object_name", bpy.data, "objects", text="Armature Object")

        layout.separator()
        layout.prop(self, "num_vertices_per_strand")
        layout.prop(self, "coordinate_space")
        layout.prop(self, "scale")
        layout.prop(self, "swap_yz")
        layout.prop(self, "flip_y")
        layout.prop(self, "flip_z")
        layout.prop(self, "lock_tip")

        layout.separator()
        layout.prop(self, "invert_uv_y")
        layout.prop(self, "export_tfxbone")
        layout.prop(self, "max_influences")
        layout.prop(self, "normalize_bone_weights")

    def _named_object(self, object_name, allowed_types):
        object_name = object_name.strip()
        if not object_name:
            return None

        obj = bpy.data.objects.get(object_name)
        if obj is None or obj.type not in allowed_types:
            return None

        return obj

    def _curve_objects(self, context):
        curve_object = self._named_object(self.curve_object_name, {"CURVE", "CURVES"})
        if self.curve_object_name.strip():
            return [curve_object] if curve_object is not None else []

        return [obj for obj in context.selected_objects if obj.type in {"CURVE", "CURVES"}]

    def _position_to_export_space(self, world_position, curve_obj, mesh_obj):
        if self.coordinate_space == "MESH_LOCAL" and mesh_obj is not None:
            return mesh_obj.matrix_world.inverted() @ world_position
        if self.coordinate_space == "CURVE_LOCAL":
            return curve_obj.matrix_world.inverted() @ world_position
        return world_position

    def execute(self, context):
        curve_objects = self._curve_objects(context)
        if not curve_objects:
            self.report({"ERROR"}, "Select a Curve/Curves object or set Curve Object")
            return {"CANCELLED"}

        mesh_obj = self._named_object(self.mesh_object_name, {"MESH"})
        if self.mesh_object_name.strip() and mesh_obj is None:
            self.report({"ERROR"}, "Mesh Object must name a Mesh object")
            return {"CANCELLED"}

        armature_obj = self._named_object(self.armature_object_name, {"ARMATURE"})
        if self.armature_object_name.strip() and armature_obj is None:
            self.report({"ERROR"}, "Armature Object must name an Armature object")
            return {"CANCELLED"}

        num_vertices = int(self.num_vertices_per_strand)
        depsgraph = context.evaluated_depsgraph_get()
        strands = []
        root_positions_world = []

        for curve_obj in curve_objects:
            object_strands = extract_object_strands(curve_obj, depsgraph)
            if not object_strands:
                self.report({"WARNING"}, "No strand data found in {}".format(curve_obj.name))
                continue

            for points in object_strands:
                export_points = [
                    self._position_to_export_space(point, curve_obj, mesh_obj)
                    for point in points
                ]
                resampled = resample_polyline(export_points, num_vertices)
                if len(resampled) == num_vertices:
                    root_positions_world.append(points[0].copy())
                    strands.append(resampled)

        if not strands:
            self.report({"ERROR"}, "No valid strands with at least two points were found")
            return {"CANCELLED"}

        if self.export_tfxbone and mesh_obj is None:
            self.report({"ERROR"}, ".tfxbone export requires Mesh Object")
            return {"CANCELLED"}

        mesh_bind_data = None
        strand_uvs = [(0.0, 0.0)] * len(strands)
        strand_bindings = [[] for _strand in strands]

        try:
            if mesh_obj is not None:
                mesh_bind_data = MeshBindData(
                    context,
                    mesh_obj,
                    armature_obj,
                    self.invert_uv_y,
                    self.max_influences,
                    self.normalize_bone_weights,
                )

                if self.export_tfxbone and not mesh_bind_data.bone_names:
                    self.report({"ERROR"}, "Mesh has no matching vertex groups/bones for .tfxbone export")
                    return {"CANCELLED"}

                strand_uvs = []
                strand_bindings = []
                for root_world in root_positions_world[: len(strands)]:
                    uv, influences = mesh_bind_data.bind_root(root_world)
                    strand_uvs.append(uv)
                    strand_bindings.append(influences)

            tfx_path = output_path_with_extension(self.filepath, ".tfx")
            write_tfx(
                tfx_path,
                strands,
                strand_uvs,
                num_vertices,
                self.scale,
                self.swap_yz,
                self.flip_y,
                self.flip_z,
                self.lock_tip,
            )

            if self.export_tfxbone:
                tfxbone_path = output_path_with_extension(self.filepath, ".tfxbone")
                write_tfxbone(tfxbone_path, mesh_bind_data.bone_names, strand_bindings)

        finally:
            if mesh_bind_data is not None:
                mesh_bind_data.cleanup()

        if self.export_tfxbone:
            self.report({"INFO"}, "Exported {} strands to .tfx and .tfxbone".format(len(strands)))
        else:
            self.report({"INFO"}, "Exported {} strands to .tfx".format(len(strands)))

        return {"FINISHED"}


def menu_func_export(self, _context):
    self.layout.operator(TRESSFX_OT_export.bl_idname, text="TressFX (.tfx)")


classes = (
    TRESSFX_OT_export,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    try:
        bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    except ValueError:
        pass

    for cls in reversed(classes):
        try:
            bpy.utils.unregister_class(cls)
        except RuntimeError:
            pass


if __name__ == "__main__":
    register()
