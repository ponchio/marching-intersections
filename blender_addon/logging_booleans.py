bl_info = {
    "name": "Boolean Audit Logger",
    "author": "Copilot",
    "version": (1, 1, 0),
    "blender": (4, 0, 0),
    "location": "View3D",
    "description": "Logs Boolean Apply operations with OBJ snapshots",
    "category": "Object",
}

import bpy
import os
import json
from datetime import datetime
from bpy.app.handlers import persistent

# ============================================================
# GLOBALS
# ============================================================

BOOLEAN_TYPES = {
    "DIFFERENCE",
    "UNION",
    "INTERSECT",
}

known_booleans = {}

known_objects = set()

known_objects_initialized = False

pending_events = []

pending_shape_snapshots = []

logged_events = set()

is_processing = False

next_id_counter = 1


# ============================================================
# PATHS
# ============================================================

def get_log_dir():

    try:
        blend_path = bpy.data.filepath
    except:
        blend_path = ""

    if blend_path:
        root = os.path.dirname(blend_path)
    else:
        root = os.path.expanduser("~")

    log_dir = os.path.join(root, "BooleanAudit")
    snap_dir = os.path.join(log_dir, "snapshots")

    os.makedirs(log_dir, exist_ok=True)
    os.makedirs(snap_dir, exist_ok=True)

    return log_dir, snap_dir


# ============================================================
# ID GENERATION
# ============================================================

def load_initial_counter():

    global next_id_counter

    log_dir, _ = get_log_dir()

    logfile = os.path.join(
        log_dir,
        "operations.json"
    )

    if not os.path.exists(logfile):

        next_id_counter = 1
        return

    try:

        with open(
            logfile,
            "r",
            encoding="utf8"
        ) as f:

            data = json.load(f)

        next_id_counter = len(data) + 1

    except Exception:

        next_id_counter = 1


def next_operation_id():

    global next_id_counter

    value = next_id_counter

    next_id_counter += 1

    return value


# ============================================================
# UTILS
# ============================================================

def matrix_to_list(matrix):

    return [
        [float(v) for v in row]
        for row in matrix
    ]

def append_log(record):

    log_dir, _ = get_log_dir()

    logfile = os.path.join(
        log_dir,
        "operations.json"
    )

    # carica il contenuto esistente
    if os.path.exists(logfile):

        try:
            with open(
                logfile,
                "r",
                encoding="utf8"
            ) as f:

                data = json.load(f)

        except Exception:

            data = []

    else:

        data = []

    # aggiunge il nuovo record
    data.append(record)

    # riscrive il file completo
    with open(
        logfile,
        "w",
        encoding="utf8"
    ) as f:

        json.dump(
            data,
            f,
            indent=2
        )


def find_object_by_pointer(ptr):

    for obj in bpy.data.objects:

        try:
            if obj.as_pointer() == ptr:
                return obj
        except:
            pass

    return None


# ============================================================
# SNAPSHOT EXPORT
# ============================================================

def export_obj(filepath, obj):

    bpy.ops.object.select_all(
        action='DESELECT'
    )

    obj.select_set(True)

    bpy.context.view_layer.objects.active = obj

    bpy.ops.wm.obj_export(
        filepath=filepath,
        export_selected_objects=True
    )


def create_worldspace_copy(obj):

    mesh = obj.data.copy()

    mesh.transform(
        obj.matrix_world
    )

    tmp_obj = bpy.data.objects.new(
        f"__AUDIT__{obj.name}",
        mesh
    )

    bpy.context.scene.collection.objects.link(
        tmp_obj
    )

    return tmp_obj


def export_worldspace_snapshot(
    obj,
    filepath
):

    tmp_obj = create_worldspace_copy(obj)

    try:

        export_obj(
            filepath,
            tmp_obj
        )

    finally:

        bpy.data.objects.remove(
            tmp_obj,
            do_unlink=True
        )


def save_shape_snapshot(obj):

    _, snap_dir = get_log_dir()
    filename = (
        f"created_{bpy.path.clean_name(obj.name)}_"
        f"{datetime.utcnow().strftime('%Y%m%d_%H%M%S_%f')}.obj"
    )
    filepath = os.path.join(snap_dir, filename)

    export_obj(
        filepath,
        obj
    )

    print(
        f"[BooleanAudit] Saved new-shape snapshot: {filepath}"
    )


# ============================================================
# RECORDING
# ============================================================

def record_boolean(info):

    target = find_object_by_pointer(
        info["target_ptr"]
    )

    tool = find_object_by_pointer(
        info["tool_ptr"]
    )

    if target is None:
        return

    if tool is None:
        return

    op_id = next_operation_id()

    _, snap_dir = get_log_dir()

    target_file = os.path.join(
        snap_dir,
        f"{op_id:06d}_target.obj"
    )

    tool_file = os.path.join(
        snap_dir,
        f"{op_id:06d}_tool.obj"
    )

    export_worldspace_snapshot(
        target,
        target_file
    )

    export_worldspace_snapshot(
        tool,
        tool_file
    )

    record = {

        "id": op_id,

        "timestamp":
            datetime.utcnow().isoformat(),

        "operation":
            info["operation"],

        "target": {

            "name":
                target.name,

            "obj_snapshot":
                os.path.basename(
                    target_file
                ),

            "matrix_world":
                matrix_to_list(
                    target.matrix_world
                ),
        },

        "tool": {

            "name":
                tool.name,

            "obj_snapshot":
                os.path.basename(
                    tool_file
                ),

            "matrix_world":
                matrix_to_list(
                    tool.matrix_world
                ),
        }
    }

    append_log(record)

    print(
        f"[BooleanAudit] "
        f"{info['operation']} "
        f"{target.name} <- {tool.name}"
    )


# ============================================================
# TRACKER
# ============================================================

@persistent
def track_booleans(dummy):

    global known_booleans

    if is_processing:
        return

    current = {}

    for obj in bpy.data.objects:

        for mod in obj.modifiers:

            if mod.type != 'BOOLEAN':
                continue

            if mod.object is None:
                continue

            if mod.operation not in BOOLEAN_TYPES:
                continue

            key = (
                obj.as_pointer(),
                mod.name,
            )

            current[key] = {

                "target_ptr":
                    obj.as_pointer(),

                "target_name":
                    obj.name,

                "modifier_name":
                    mod.name,

                "operation":
                    mod.operation,

                "solver":
                    getattr(
                        mod,
                        "solver",
                        "EXACT"
                    ),

                "tool_ptr":
                    mod.object.as_pointer(),

                "tool_name":
                    mod.object.name,
            }

    disappeared = (
        set(known_booleans.keys())
        - set(current.keys())
    )

    for key in disappeared:

        info = known_booleans[key]

        event_key = (
            info["target_ptr"],
            info["modifier_name"],
            info["tool_ptr"],
            info["operation"]
        )

        if event_key in logged_events:
            continue

        logged_events.add(
            event_key
        )

        pending_events.append(
            info
        )

    known_booleans = current


@persistent
def reset_known_objects(dummy):

    global known_objects
    global known_objects_initialized

    known_objects = set(bpy.data.objects)
    known_objects_initialized = True
    pending_shape_snapshots.clear()

    if (
        check_new_shapes
        not in bpy.app.handlers.depsgraph_update_post
    ):
        bpy.app.handlers.depsgraph_update_post.append(
            check_new_shapes
        )


def initialize_shape_tracking():

    global known_objects
    global known_objects_initialized

    if not known_objects_initialized:
        known_objects = set(bpy.data.objects)
        known_objects_initialized = True

    if (
        check_new_shapes
        not in bpy.app.handlers.depsgraph_update_post
    ):
        bpy.app.handlers.depsgraph_update_post.append(
            check_new_shapes
        )


@persistent
def check_new_shapes(scene, depsgraph):

    if is_processing or not known_objects_initialized:
        return

    current_objects = set(bpy.data.objects)
    added_objects = current_objects - known_objects

    for obj in added_objects:
        if obj.type in {'MESH', 'CURVE', 'SURFACE', 'META', 'FONT'}:
            pending_shape_snapshots.append(obj.as_pointer())

    known_objects.clear()
    known_objects.update(current_objects)


# ============================================================
# EVENT QUEUE
# ============================================================

def process_pending_events():

    global is_processing

    if is_processing:
        return 0.5

    if not pending_events and not pending_shape_snapshots:
        return 0.5

    is_processing = True

    try:

        if pending_shape_snapshots:
            obj = find_object_by_pointer(
                pending_shape_snapshots.pop(0)
            )

            if obj is not None:
                save_shape_snapshot(obj)
        else:
            event = pending_events.pop(0)
            record_boolean(event)

    except Exception as e:

        print(
            "[BooleanAudit]",
            str(e)
        )

    finally:

        is_processing = False

    return 0.5


# ============================================================
# REGISTER
# ============================================================

def register():

    global known_booleans
    global known_objects
    global known_objects_initialized

    known_booleans = {}
    known_objects = set()
    known_objects_initialized = False
    pending_shape_snapshots.clear()

    load_initial_counter()

    if (
        track_booleans
        not in bpy.app.handlers.depsgraph_update_post
    ):
        bpy.app.handlers.depsgraph_update_post.append(
            track_booleans
        )

    if (
        reset_known_objects
        not in bpy.app.handlers.load_post
    ):
        bpy.app.handlers.load_post.append(
            reset_known_objects
        )

    if not bpy.app.timers.is_registered(initialize_shape_tracking):
        bpy.app.timers.register(
            initialize_shape_tracking,
            first_interval=0.0
        )

    bpy.app.timers.register(
        process_pending_events,
        persistent=True
    )

    print(
        "[BooleanAudit] registered"
    )


def unregister():

    global known_booleans
    global known_objects

    if (
        track_booleans
        in bpy.app.handlers.depsgraph_update_post
    ):
        bpy.app.handlers.depsgraph_update_post.remove(
            track_booleans
        )

    if (
        check_new_shapes
        in bpy.app.handlers.depsgraph_update_post
    ):
        bpy.app.handlers.depsgraph_update_post.remove(
            check_new_shapes
        )

    if (
        reset_known_objects
        in bpy.app.handlers.load_post
    ):
        bpy.app.handlers.load_post.remove(
            reset_known_objects
        )

    if bpy.app.timers.is_registered(initialize_shape_tracking):
        bpy.app.timers.unregister(initialize_shape_tracking)

    known_booleans.clear()
    known_objects.clear()

    pending_events.clear()
    pending_shape_snapshots.clear()

    logged_events.clear()

    print(
        "[BooleanAudit] unregistered"
    )


if __name__ == "__main__":
    register()
