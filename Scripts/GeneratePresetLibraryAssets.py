"""Generate XTools preset CurveVector and CameraShake assets.

Run with UE 5.3 or later, for example:
UnrealEditor.exe <Project>.uproject -ExecutePythonScript=<ThisFile>

The Python Editor Script Plugin is required only when regenerating the assets.
The generated assets are native Unreal assets and have no Python runtime
dependency.
"""

import json
import os

import unreal


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PRESET_FILE = os.path.join(SCRIPT_DIR, "PresetLibraryAssets.json")
SOURCE_DIR = os.path.abspath(
    os.path.join(unreal.Paths.project_saved_dir(), "XTools", "PresetLibraryImport")
)
VALIDATE_ONLY = os.environ.get("XTOOLS_PRESET_LIBRARY_VALIDATE_ONLY") == "1"
LOCATION_CHANNELS = ("x", "y", "z")
ROTATION_CHANNELS = ("pitch", "yaw", "roll")
ALL_SHAKE_CHANNELS = LOCATION_CHANNELS + ROTATION_CHANNELS + ("fov",)
CURVE_SEGMENT_REFERENCE_STEPS = 256
MAX_VECTOR_CURVE_ERROR = 0.0005


def _load_presets():
    with open(PRESET_FILE, "r", encoding="utf-8") as preset_file:
        return json.load(preset_file)


def _asset_path(directory, name):
    return f"{directory}/{name}"


def _make_directory(directory):
    if not unreal.EditorAssetLibrary.does_directory_exist(directory):
        unreal.EditorAssetLibrary.make_directory(directory)


def _delete_existing_asset(path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.delete_asset(path):
            raise RuntimeError(f"Failed to delete existing asset: {path}")


def _set_property(obj, property_name, value):
    try:
        obj.set_editor_property(property_name, value)
    except Exception as exc:
        raise RuntimeError(
            f"Failed to set {obj.get_name()}.{property_name} to {value!r}: {exc}"
        ) from exc


def _set_bool_property(obj, possible_names, value):
    for property_name in possible_names:
        try:
            obj.set_editor_property(property_name, value)
            return
        except Exception:
            pass
    raise RuntimeError(f"Failed to set any boolean property {possible_names} on {obj.get_name()}")


def _get_bool_property(obj, possible_names):
    for property_name in possible_names:
        try:
            return bool(obj.get_editor_property(property_name))
        except Exception:
            pass
    raise RuntimeError(f"Failed to get any boolean property {possible_names} on {obj.get_name()}")


def _smoothstep(alpha):
    return alpha * alpha * (3.0 - 2.0 * alpha)


def _evaluate_anchor_segment(start, end, alpha):
    eased_alpha = _smoothstep(alpha)
    return tuple(
        float(start[index]) + (float(end[index]) - float(start[index])) * eased_alpha
        for index in range(1, 4)
    )


def _build_curve_vector_samples(keys):
    samples = []
    for segment_index in range(len(keys) - 1):
        start = keys[segment_index]
        end = keys[segment_index + 1]
        segment_samples = []
        for index in range(CURVE_SEGMENT_REFERENCE_STEPS + 1):
            alpha = index / CURVE_SEGMENT_REFERENCE_STEPS
            time = float(start[0]) + (float(end[0]) - float(start[0])) * alpha
            segment_samples.append((time,) + _evaluate_anchor_segment(start, end, alpha))

        keep = {0, CURVE_SEGMENT_REFERENCE_STEPS}
        pending = [(0, CURVE_SEGMENT_REFERENCE_STEPS)]
        while pending:
            start_index, end_index = pending.pop()
            start_sample = segment_samples[start_index]
            end_sample = segment_samples[end_index]
            worst_error = -1.0
            worst_index = -1

            for index in range(start_index + 1, end_index):
                alpha = (index - start_index) / (end_index - start_index)
                linear_values = tuple(
                    start_sample[axis] + (end_sample[axis] - start_sample[axis]) * alpha
                    for axis in range(1, 4)
                )
                error = max(
                    abs(segment_samples[index][axis] - linear_values[axis - 1])
                    for axis in range(1, 4)
                )
                if error > worst_error:
                    worst_error = error
                    worst_index = index

            if worst_error > MAX_VECTOR_CURVE_ERROR:
                keep.add(worst_index)
                pending.append((start_index, worst_index))
                pending.append((worst_index, end_index))

        simplified = [segment_samples[index] for index in sorted(keep)]
        samples.extend(simplified if segment_index == 0 else simplified[1:])

    return samples


def _validate_curve_vector(curve, preset):
    name = preset["name"]
    if not isinstance(curve, unreal.CurveVector):
        raise RuntimeError(f"{name} is not a CurveVector")

    keys = preset["keys"]
    max_error = 0.0
    for segment_index in range(len(keys) - 1):
        start = keys[segment_index]
        end = keys[segment_index + 1]
        for index in range(CURVE_SEGMENT_REFERENCE_STEPS + 1):
            alpha = index / CURVE_SEGMENT_REFERENCE_STEPS
            time = float(start[0]) + (float(end[0]) - float(start[0])) * alpha
            expected = _evaluate_anchor_segment(start, end, alpha)
            value = curve.get_vector_value(time)
            max_error = max(
                max_error,
                abs(value.x - expected[0]),
                abs(value.y - expected[1]),
                abs(value.z - expected[2]),
            )

    if max_error > MAX_VECTOR_CURVE_ERROR + 1.0e-6:
        raise RuntimeError(f"{name} exceeds vector curve error limit: {max_error}")

    return max_error


def _create_curve_vector(asset_tools, directory, preset):
    name = preset["name"]
    keys = preset["keys"]
    if len(keys) < 2:
        raise RuntimeError(f"{name} must contain at least two keys")
    if any(len(key) != 4 for key in keys):
        raise RuntimeError(f"{name} must use [time, x, y, z] keys")
    if any(float(keys[index][0]) >= float(keys[index + 1][0]) for index in range(len(keys) - 1)):
        raise RuntimeError(f"{name} key times must be strictly increasing")
    samples = _build_curve_vector_samples(keys)

    source_filename = os.path.join(SOURCE_DIR, f"{name}.csv")
    with open(source_filename, "w", encoding="utf-8", newline="") as source_file:
        for sample in samples:
            source_file.write("{0:.9g},{1:.9g},{2:.9g},{3:.9g}\n".format(*sample))

    settings = unreal.CSVImportSettings()
    settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_CURVE_VECTOR)
    settings.set_editor_property("import_curve_interp_mode", unreal.RichCurveInterpMode.RCIM_LINEAR)

    factory = unreal.CSVImportFactory()
    factory.set_editor_property("automated_import_settings", settings)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_filename)
    task.set_editor_property("destination_path", directory)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", factory)

    asset_tools.import_asset_tasks([task])
    imported_objects = task.get_objects()
    if len(imported_objects) != 1 or not isinstance(imported_objects[0], unreal.CurveVector):
        raise RuntimeError(f"Failed to import CurveVector {directory}/{name}")

    curve = imported_objects[0]
    max_error = _validate_curve_vector(curve, preset)

    unreal.log(
        f"Generated {name}: {len(keys)} anchors, {len(samples)} vector keys, max error {max_error:.7f}"
    )
    return curve


def _get_default_object(generated_class):
    return unreal.get_default_object(generated_class)


def _is_generated_class(candidate):
    return bool(candidate and hasattr(candidate, "get_name") and candidate.get_name() != "BlueprintGeneratedClass")


def _get_generated_class(blueprint, asset_path=None):
    if asset_path:
        asset_name = asset_path.rsplit("/", 1)[-1]
        generated_class_path = f"{asset_path}.{asset_name}_C"

        load_class = getattr(unreal, "load_class", None)
        if callable(load_class):
            generated_class = load_class(None, generated_class_path)
            if _is_generated_class(generated_class):
                return generated_class

        load_object = getattr(unreal, "load_object", None)
        if callable(load_object):
            generated_class = load_object(None, generated_class_path)
            if _is_generated_class(generated_class):
                return generated_class

    if asset_path and hasattr(unreal.EditorAssetLibrary, "load_blueprint_class"):
        generated_class = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
        if _is_generated_class(generated_class):
            return generated_class

    try:
        generated_class = blueprint.get_editor_property("generated_class")
        if _is_generated_class(generated_class):
            return generated_class
    except Exception:
        pass

    generated_class = getattr(blueprint, "generated_class", None)
    if callable(generated_class):
        generated_class = generated_class()
    if _is_generated_class(generated_class):
        return generated_class

    get_blueprint_class = getattr(blueprint, "get_blueprint_class", None)
    if callable(get_blueprint_class):
        generated_class = get_blueprint_class()
        if _is_generated_class(generated_class):
            return generated_class

    raise RuntimeError(f"Failed to resolve generated class for {blueprint.get_name()}")


def _compile_blueprint(blueprint):
    if hasattr(unreal, "KismetEditorUtilities"):
        unreal.KismetEditorUtilities.compile_blueprint(blueprint)
        return
    if hasattr(unreal, "BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        return
    raise RuntimeError("No Blueprint compile utility is available in Python")


def _find_root_pattern(camera_shake_cdo):
    if hasattr(camera_shake_cdo, "get_root_shake_pattern"):
        root = camera_shake_cdo.get_root_shake_pattern()
        if root:
            return root

    try:
        root = camera_shake_cdo.get_editor_property("root_shake_pattern")
        if root:
            return root
    except Exception:
        pass

    raise RuntimeError(f"{camera_shake_cdo.get_name()} has no root shake pattern")


def _set_shaker(pattern, property_name, values):
    if not values:
        return
    shaker = pattern.get_editor_property(property_name)
    _set_property(shaker, "amplitude", float(values[0]))
    _set_property(shaker, "frequency", float(values[1]))
    _set_property(pattern, property_name, shaker)


def _validate_shaker(pattern, property_name, expected_values):
    shaker = pattern.get_editor_property(property_name)
    amplitude = float(shaker.get_editor_property("amplitude"))
    frequency = float(shaker.get_editor_property("frequency"))
    if abs(amplitude - float(expected_values[0])) > 1.0e-4:
        raise RuntimeError(f"{pattern.get_name()}.{property_name} amplitude did not round-trip")
    if abs(frequency - float(expected_values[1])) > 1.0e-4:
        raise RuntimeError(f"{pattern.get_name()}.{property_name} frequency did not round-trip")


def _configure_perlin_pattern(pattern, preset):
    _set_property(pattern, "duration", float(preset["duration"]))
    _set_property(pattern, "blend_in_time", float(preset.get("blend_in", 0.0)))
    _set_property(pattern, "blend_out_time", float(preset.get("blend_out", 0.0)))

    for channel in ALL_SHAKE_CHANNELS:
        _set_shaker(pattern, channel, (0.0, 1.0))

    for axis, values in preset.get("location", {}).items():
        _set_shaker(pattern, axis, values)

    for axis, values in preset.get("rotation", {}).items():
        _set_shaker(pattern, axis, values)

    if "fov" in preset:
        _set_shaker(pattern, "fov", preset["fov"])


def _validate_camera_shake(blueprint, path, preset):
    name = preset["name"]
    if not isinstance(blueprint, unreal.Blueprint):
        raise RuntimeError(f"{name} is not a Blueprint asset")

    cdo = _get_default_object(_get_generated_class(blueprint, path))
    expected_single_instance = bool(preset.get("single_instance", True))
    actual_single_instance = _get_bool_property(cdo, ("single_instance", "b_single_instance"))
    if actual_single_instance != expected_single_instance:
        raise RuntimeError(f"{name} single-instance setting did not round-trip")

    root_pattern = _find_root_pattern(cdo)
    if not isinstance(root_pattern, unreal.PerlinNoiseCameraShakePattern):
        raise RuntimeError(f"{name} did not reload with Perlin root pattern")

    for property_name, expected in (
        ("duration", preset["duration"]),
        ("blend_in_time", preset.get("blend_in", 0.0)),
        ("blend_out_time", preset.get("blend_out", 0.0)),
    ):
        actual = float(root_pattern.get_editor_property(property_name))
        if abs(actual - float(expected)) > 1.0e-4:
            raise RuntimeError(f"{name} {property_name} did not round-trip: {actual}")

    expected_channels = {}
    expected_channels.update(preset.get("location", {}))
    expected_channels.update(preset.get("rotation", {}))
    if "fov" in preset:
        expected_channels["fov"] = preset["fov"]

    for channel in ALL_SHAKE_CHANNELS:
        _validate_shaker(root_pattern, channel, expected_channels.get(channel, (0.0, 1.0)))

    return root_pattern


def _create_camera_shake(asset_tools, directory, preset):
    name = preset["name"]
    path = _asset_path(directory, name)
    _delete_existing_asset(path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.DefaultCameraShakeBase.static_class())

    blueprint = asset_tools.create_asset(name, directory, unreal.Blueprint.static_class(), factory)
    if not blueprint:
        raise RuntimeError(f"Failed to create CameraShake Blueprint {path}")

    _compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        raise RuntimeError(f"Failed to save initial CameraShake Blueprint {path}")

    cdo = _get_default_object(_get_generated_class(blueprint, path))
    _set_bool_property(cdo, ("single_instance", "b_single_instance"), bool(preset.get("single_instance", True)))

    root_pattern = _find_root_pattern(cdo)
    if not isinstance(root_pattern, unreal.PerlinNoiseCameraShakePattern):
        raise RuntimeError(f"{name} root pattern is {root_pattern.get_class().get_name()}, expected Perlin")

    _configure_perlin_pattern(root_pattern, preset)
    _compile_blueprint(blueprint)

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        raise RuntimeError(f"Failed to save CameraShake Blueprint {path}")

    reloaded = unreal.EditorAssetLibrary.load_asset(path)
    reloaded_root = _validate_camera_shake(reloaded, path, preset)
    duration = reloaded_root.get_editor_property("duration")

    unreal.log(f"Generated {name}: Perlin CameraShake duration {duration:.3f}s")
    return reloaded


def main():
    presets = _load_presets()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    os.makedirs(SOURCE_DIR, exist_ok=True)

    curve_path = presets["curve_vector_path"]
    shake_path = presets["camera_shake_path"]
    _make_directory(curve_path)
    _make_directory(shake_path)

    if VALIDATE_ONLY:
        for preset in presets["scale_curves"]:
            path = _asset_path(curve_path, preset["name"])
            _validate_curve_vector(unreal.EditorAssetLibrary.load_asset(path), preset)

        for preset in presets["camera_shakes"]:
            path = _asset_path(shake_path, preset["name"])
            blueprint = unreal.EditorAssetLibrary.load_asset(path)
            _validate_camera_shake(blueprint, path, preset)

        unreal.log(
            "Validated preset library assets: {0} CurveVector assets, {1} CameraShake assets".format(
                len(presets["scale_curves"]), len(presets["camera_shakes"])
            )
        )
        return

    generated_curves = []
    for preset in presets["scale_curves"]:
        generated_curves.append(_create_curve_vector(asset_tools, curve_path, preset))

    generated_shakes = []
    for preset in presets["camera_shakes"]:
        generated_shakes.append(_create_camera_shake(asset_tools, shake_path, preset))

    unreal.log(
        "Generated preset library assets: {0} CurveVector assets, {1} CameraShake assets".format(
            len(generated_curves), len(generated_shakes)
        )
    )


if __name__ == "__main__":
    main()
