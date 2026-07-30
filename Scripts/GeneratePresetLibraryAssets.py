"""Generate XTools preset CurveFloat, CurveVector, and CameraShake assets.

Run with UE 5.3 or later, for example:
UnrealEditor.exe <Project>.uproject -ExecutePythonScript=<ThisFile>

The Python Editor Script Plugin is required only when regenerating the assets.
The generated assets are native Unreal assets and have no Python runtime
dependency.
"""

import json
import math
import os
import posixpath

import unreal


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PRESET_FILE = os.path.join(SCRIPT_DIR, "PresetLibraryAssets.json")
SOURCE_DIR = os.path.abspath(
    os.path.join(unreal.Paths.project_saved_dir(), "XTools", "PresetLibraryImport")
)
VALIDATE_ONLY = os.environ.get("XTOOLS_PRESET_LIBRARY_VALIDATE_ONLY") == "1"
FORCE_REBUILD = os.environ.get("XTOOLS_PRESET_LIBRARY_FORCE_REBUILD") == "1"
LOCATION_CHANNELS = ("x", "y", "z")
ROTATION_CHANNELS = ("pitch", "yaw", "roll")
ALL_SHAKE_CHANNELS = LOCATION_CHANNELS + ROTATION_CHANNELS + ("fov",)
SHAKE_MULTIPLIER_PROPERTIES = (
    "location_amplitude_multiplier",
    "location_frequency_multiplier",
    "rotation_amplitude_multiplier",
    "rotation_frequency_multiplier",
)
WAVE_OFFSET_TYPES = {
    "random": unreal.InitialWaveOscillatorOffsetType.RANDOM,
    "zero": unreal.InitialWaveOscillatorOffsetType.ZERO,
}
CURVE_SEGMENT_REFERENCE_STEPS = 256
MAX_VECTOR_CURVE_ERROR = 0.0005
MAX_FLOAT_CURVE_ERROR = 0.0005
VECTOR_COMPONENT_NAMES = ("X", "Y", "Z")


def _load_presets():
    with open(PRESET_FILE, "r", encoding="utf-8") as preset_file:
        return json.load(preset_file)


def _asset_path(directory, name):
    return f"{directory}/{name}"


def _validate_asset_directory(directory):
    if not isinstance(directory, str) or not directory.startswith("/XTools/"):
        raise RuntimeError(f"Preset path must stay under /XTools/: {directory!r}")
    if "\\" in directory:
        raise RuntimeError(f"Preset path must use Unreal package separators: {directory!r}")

    normalized_directory = posixpath.normpath(directory)
    if directory not in (normalized_directory, normalized_directory + "/"):
        raise RuntimeError(f"Preset path contains an invalid segment: {directory!r}")
    if normalized_directory == "/XTools" or any(
        not segment for segment in normalized_directory.split("/")[1:]
    ):
        raise RuntimeError(f"Preset path contains an empty segment: {directory!r}")
    return normalized_directory


def _validate_asset_identity(directory, name):
    directory = _validate_asset_directory(directory)
    if not isinstance(name, str) or not name or "/" in name or "\\" in name:
        raise RuntimeError(f"Invalid preset asset name: {name!r}")
    return _asset_path(directory.rstrip("/"), name)


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


def _evaluate_cubic_segment(start, end, alpha):
    duration = float(end[0]) - float(start[0])
    alpha_squared = alpha * alpha
    alpha_cubed = alpha_squared * alpha
    return (
        (2.0 * alpha_cubed - 3.0 * alpha_squared + 1.0) * float(start[1])
        + (alpha_cubed - 2.0 * alpha_squared + alpha) * duration * float(start[3])
        + (-2.0 * alpha_cubed + 3.0 * alpha_squared) * float(end[1])
        + (alpha_cubed - alpha_squared) * duration * float(end[2])
    )


def _validate_curve_float_preset(preset):
    name = preset["name"]
    keys = preset["keys"]
    if len(keys) < 2:
        raise RuntimeError(f"{name} must contain at least two keys")

    for key_index, key in enumerate(keys):
        if len(key) != 4:
            raise RuntimeError(
                f"{name} key {key_index} must use "
                "[time, value, arrive_tangent, leave_tangent]"
            )
        if not all(math.isfinite(float(value)) for value in key):
            raise RuntimeError(f"{name} key {key_index} contains a non-finite value")

    if any(float(keys[index][0]) >= float(keys[index + 1][0]) for index in range(len(keys) - 1)):
        raise RuntimeError(f"{name} key times must be strictly increasing")
    return keys


def _validate_curve_float(curve, preset):
    name = preset["name"]
    if not isinstance(curve, unreal.CurveFloat):
        raise RuntimeError(f"{name} is not a CurveFloat")

    keys = _validate_curve_float_preset(preset)
    max_error = 0.0
    for segment_index in range(len(keys) - 1):
        start = keys[segment_index]
        end = keys[segment_index + 1]
        for index in range(CURVE_SEGMENT_REFERENCE_STEPS + 1):
            alpha = index / CURVE_SEGMENT_REFERENCE_STEPS
            time = float(start[0]) + (float(end[0]) - float(start[0])) * alpha
            expected = _evaluate_cubic_segment(start, end, alpha)
            actual = float(curve.get_float_value(time))
            if not math.isfinite(actual):
                raise RuntimeError(f"{name} contains a non-finite value at time {time}")
            max_error = max(max_error, abs(actual - expected))

    if max_error > MAX_FLOAT_CURVE_ERROR + 1.0e-6:
        raise RuntimeError(f"{name} exceeds float curve error limit: {max_error}")
    key_data = [
        unreal.Vector4(x=time, y=value, z=arrive, w=leave)
        for time, value, arrive, leave in keys
    ]
    if not unreal.XCurvePresetLibrary.does_curve_float_match_cubic_keys(curve, key_data):
        raise RuntimeError(f"{name} does not contain the expected cubic keys and tangents")
    return max_error


def _validate_curve_vector_preset(preset):
    name = preset["name"]
    keys = preset["keys"]
    if len(keys) < 2:
        raise RuntimeError(f"{name} must contain at least two keys")

    for key_index, key in enumerate(keys):
        if len(key) != 4:
            raise RuntimeError(f"{name} key {key_index} must use [time, x, y, z]")
        if not all(math.isfinite(float(value)) for value in key):
            raise RuntimeError(f"{name} key {key_index} contains a non-finite value")

    if any(float(keys[index][0]) >= float(keys[index + 1][0]) for index in range(len(keys) - 1)):
        raise RuntimeError(f"{name} key times must be strictly increasing")
    return keys


def _validate_curve_vector(curve, preset):
    name = preset["name"]
    if not isinstance(curve, unreal.CurveVector):
        raise RuntimeError(f"{name} is not a CurveVector")

    keys = _validate_curve_vector_preset(preset)
    max_error = 0.0
    for segment_index in range(len(keys) - 1):
        start = keys[segment_index]
        end = keys[segment_index + 1]
        for index in range(CURVE_SEGMENT_REFERENCE_STEPS + 1):
            alpha = index / CURVE_SEGMENT_REFERENCE_STEPS
            time = float(start[0]) + (float(end[0]) - float(start[0])) * alpha
            expected = _evaluate_anchor_segment(start, end, alpha)
            value = curve.get_vector_value(time)
            actual = (value.x, value.y, value.z)
            for axis, axis_name in enumerate(VECTOR_COMPONENT_NAMES):
                if not math.isfinite(float(actual[axis])):
                    raise RuntimeError(
                        f"{name} contains a non-finite {axis_name}-axis value at time {time}"
                    )
            max_error = max(
                max_error,
                *(abs(actual[axis] - expected[axis]) for axis in range(3)),
            )

    if max_error > MAX_VECTOR_CURVE_ERROR + 1.0e-6:
        raise RuntimeError(f"{name} exceeds vector curve error limit: {max_error}")
    key_data = [unreal.Vector4(x=time, y=x, z=y, w=z) for time, x, y, z in keys]
    if not unreal.XCurvePresetLibrary.does_curve_vector_match_cubic_keys(curve, key_data):
        raise RuntimeError(f"{name} does not contain the expected cubic vector keys")

    return max_error


def _create_curve_float(asset_tools, preset):
    directory = preset["path"]
    name = preset["name"]
    keys = _validate_curve_float_preset(preset)
    path = _asset_path(directory, name)

    _make_directory(directory)
    if not FORCE_REBUILD and unreal.EditorAssetLibrary.does_asset_exist(path):
        existing_curve = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(existing_curve, unreal.CurveFloat):
            raise RuntimeError(
                f"{path} is not a CurveFloat; use Force only if replacement is intentional"
            )
        try:
            max_error = _validate_curve_float(existing_curve, preset)
        except RuntimeError as exc:
            unreal.log_warning(f"Rebuilding {name}: {exc}")
        else:
            unreal.log(f"Unchanged {name}: max error {max_error:.7f}")
            return existing_curve

    source_filename = os.path.join(SOURCE_DIR, f"{name}.csv")
    with open(source_filename, "w", encoding="utf-8", newline="") as source_file:
        for time, value, _arrive_tangent, _leave_tangent in keys:
            source_file.write(f"{time:.9g},{value:.9g}\n")

    settings = unreal.CSVImportSettings()
    settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_CURVE_FLOAT)
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
    if len(imported_objects) != 1 or not isinstance(imported_objects[0], unreal.CurveFloat):
        raise RuntimeError(f"Failed to import CurveFloat {directory}/{name}")

    curve = imported_objects[0]
    key_data = [
        unreal.Vector4(x=time, y=value, z=arrive, w=leave)
        for time, value, arrive, leave in keys
    ]
    if not unreal.XCurvePresetLibrary.set_curve_float_cubic_keys(curve, key_data):
        raise RuntimeError(f"Failed to set cubic keys on {directory}/{name}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(curve, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save {directory}/{name}")
    max_error = _validate_curve_float(curve, preset)

    unreal.log(
        f"Generated {name}: {len(keys)} cubic float keys, max error {max_error:.7f}"
    )
    return curve


def _create_curve_vector(asset_tools, directory, preset):
    name = preset["name"]
    keys = _validate_curve_vector_preset(preset)
    path = _asset_path(directory, name)

    if not FORCE_REBUILD and unreal.EditorAssetLibrary.does_asset_exist(path):
        existing_curve = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(existing_curve, unreal.CurveVector):
            raise RuntimeError(
                f"{path} is not a CurveVector; use Force only if replacement is intentional"
            )
        try:
            max_error = _validate_curve_vector(existing_curve, preset)
        except RuntimeError as exc:
            unreal.log_warning(f"Rebuilding {name}: {exc}")
        else:
            unreal.log(
                f"Unchanged {name}: max error {max_error:.7f}, "
                "cubic smoothness passed"
            )
            return existing_curve

    source_filename = os.path.join(SOURCE_DIR, f"{name}.csv")
    with open(source_filename, "w", encoding="utf-8", newline="") as source_file:
        for key in keys:
            source_file.write("{0:.9g},{1:.9g},{2:.9g},{3:.9g}\n".format(*key))

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
    key_data = [unreal.Vector4(x=time, y=x, z=y, w=z) for time, x, y, z in keys]
    if not unreal.XCurvePresetLibrary.set_curve_vector_cubic_keys(curve, key_data):
        raise RuntimeError(f"Failed to set cubic keys on {directory}/{name}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(curve, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save {directory}/{name}")
    max_error = _validate_curve_vector(curve, preset)

    unreal.log(
        f"Generated {name}: {len(keys)} cubic vector keys, max error {max_error:.7f}, "
        "cubic smoothness passed"
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


def _get_pattern_type(preset):
    return preset.get("pattern", "perlin").lower()


def _get_pattern_class(pattern_type):
    if pattern_type == "perlin":
        return unreal.PerlinNoiseCameraShakePattern
    if pattern_type == "wave":
        return unreal.WaveOscillatorCameraShakePattern
    raise RuntimeError(f"Unsupported CameraShake pattern: {pattern_type!r}")


def _set_perlin_shaker(pattern, property_name, values):
    if not values:
        return
    shaker = pattern.get_editor_property(property_name)
    _set_property(shaker, "amplitude", float(values[0]))
    _set_property(shaker, "frequency", float(values[1]))
    _set_property(pattern, property_name, shaker)


def _validate_perlin_shaker(pattern, property_name, expected_values):
    shaker = pattern.get_editor_property(property_name)
    amplitude = float(shaker.get_editor_property("amplitude"))
    frequency = float(shaker.get_editor_property("frequency"))
    if abs(amplitude - float(expected_values[0])) > 1.0e-4:
        raise RuntimeError(f"{pattern.get_name()}.{property_name} amplitude did not round-trip")
    if abs(frequency - float(expected_values[1])) > 1.0e-4:
        raise RuntimeError(f"{pattern.get_name()}.{property_name} frequency did not round-trip")


def _set_wave_oscillator(pattern, property_name, values):
    oscillator = pattern.get_editor_property(property_name)
    _set_property(oscillator, "amplitude", float(values[0]))
    _set_property(oscillator, "frequency", float(values[1]))
    _set_property(oscillator, "initial_offset_type", WAVE_OFFSET_TYPES[values[2]])
    _set_property(pattern, property_name, oscillator)


def _validate_wave_oscillator(pattern, property_name, expected_values):
    oscillator = pattern.get_editor_property(property_name)
    amplitude = float(oscillator.get_editor_property("amplitude"))
    frequency = float(oscillator.get_editor_property("frequency"))
    offset_type = oscillator.get_editor_property("initial_offset_type")
    if abs(amplitude - float(expected_values[0])) > 1.0e-4:
        raise RuntimeError(f"{pattern.get_name()}.{property_name} amplitude did not round-trip")
    if abs(frequency - float(expected_values[1])) > 1.0e-4:
        raise RuntimeError(f"{pattern.get_name()}.{property_name} frequency did not round-trip")
    if offset_type != WAVE_OFFSET_TYPES[expected_values[2]]:
        raise RuntimeError(f"{pattern.get_name()}.{property_name} offset did not round-trip")


def _configure_simple_pattern(pattern, preset):
    _set_property(pattern, "duration", float(preset["duration"]))
    _set_property(pattern, "blend_in_time", float(preset.get("blend_in", 0.0)))
    _set_property(pattern, "blend_out_time", float(preset.get("blend_out", 0.0)))
    for property_name in SHAKE_MULTIPLIER_PROPERTIES:
        _set_property(pattern, property_name, float(preset.get(property_name, 1.0)))


def _configure_perlin_pattern(pattern, preset):
    _configure_simple_pattern(pattern, preset)
    for channel in ALL_SHAKE_CHANNELS:
        _set_perlin_shaker(pattern, channel, (0.0, 1.0))

    for axis, values in preset.get("location", {}).items():
        _set_perlin_shaker(pattern, axis, values)

    for axis, values in preset.get("rotation", {}).items():
        _set_perlin_shaker(pattern, axis, values)

    if "fov" in preset:
        _set_perlin_shaker(pattern, "fov", preset["fov"])


def _configure_wave_pattern(pattern, preset):
    _configure_simple_pattern(pattern, preset)
    for channel in ALL_SHAKE_CHANNELS:
        _set_wave_oscillator(pattern, channel, (0.0, 1.0, "random"))

    for axis, values in preset.get("location", {}).items():
        _set_wave_oscillator(pattern, axis, values)

    for axis, values in preset.get("rotation", {}).items():
        _set_wave_oscillator(pattern, axis, values)

    if "fov" in preset:
        _set_wave_oscillator(pattern, "fov", preset["fov"])


def _validate_camera_shake(blueprint, path, preset):
    name = preset["name"]
    if not isinstance(blueprint, unreal.Blueprint):
        raise RuntimeError(f"{name} is not a Blueprint asset")

    cdo = _get_default_object(_get_generated_class(blueprint, path))
    expected_single_instance = bool(preset.get("single_instance", True))
    actual_single_instance = _get_bool_property(cdo, ("single_instance", "b_single_instance"))
    if actual_single_instance != expected_single_instance:
        raise RuntimeError(f"{name} single-instance setting did not round-trip")

    pattern_type = _get_pattern_type(preset)
    expected_pattern_class = _get_pattern_class(pattern_type)
    root_pattern = _find_root_pattern(cdo)
    if not isinstance(root_pattern, expected_pattern_class):
        raise RuntimeError(f"{name} did not reload with {pattern_type} root pattern")

    expected_properties = [
        ("duration", preset["duration"]),
        ("blend_in_time", preset.get("blend_in", 0.0)),
        ("blend_out_time", preset.get("blend_out", 0.0)),
    ]
    expected_properties.extend(
        (property_name, preset.get(property_name, 1.0))
        for property_name in SHAKE_MULTIPLIER_PROPERTIES
    )
    for property_name, expected in expected_properties:
        actual = float(root_pattern.get_editor_property(property_name))
        if abs(actual - float(expected)) > 1.0e-4:
            raise RuntimeError(f"{name} {property_name} did not round-trip: {actual}")

    expected_channels = {}
    expected_channels.update(preset.get("location", {}))
    expected_channels.update(preset.get("rotation", {}))
    if "fov" in preset:
        expected_channels["fov"] = preset["fov"]

    if pattern_type == "perlin":
        for channel in ALL_SHAKE_CHANNELS:
            _validate_perlin_shaker(
                root_pattern, channel, expected_channels.get(channel, (0.0, 1.0))
            )
    else:
        for channel in ALL_SHAKE_CHANNELS:
            _validate_wave_oscillator(
                root_pattern, channel, expected_channels.get(channel, (0.0, 1.0, "random"))
            )

    return root_pattern


def _validate_camera_shake_preset(preset):
    name = preset["name"]
    pattern_type = _get_pattern_type(preset)
    _get_pattern_class(pattern_type)
    if "single_instance" in preset and not isinstance(preset["single_instance"], bool):
        raise RuntimeError(f"{name}.single_instance must be a JSON boolean")
    duration = float(preset["duration"])
    blend_in = float(preset.get("blend_in", 0.0))
    blend_out = float(preset.get("blend_out", 0.0))
    if not all(math.isfinite(value) for value in (duration, blend_in, blend_out)):
        raise RuntimeError(f"{name} contains a non-finite CameraShake duration")
    if blend_in < 0.0 or blend_out < 0.0:
        raise RuntimeError(f"{name} has an invalid CameraShake duration or blend time")
    if duration > 0.0 and blend_in + blend_out > duration:
        raise RuntimeError(f"{name} blend-in and blend-out exceed its duration")

    for property_name in SHAKE_MULTIPLIER_PROPERTIES:
        value = float(preset.get(property_name, 1.0))
        if not math.isfinite(value) or value < 0.0:
            raise RuntimeError(f"{name}.{property_name} must be finite and non-negative")

    for group_name, allowed_channels in (
        ("location", LOCATION_CHANNELS),
        ("rotation", ROTATION_CHANNELS),
    ):
        channels = preset.get(group_name, {})
        unknown_channels = set(channels) - set(allowed_channels)
        if unknown_channels:
            raise RuntimeError(f"{name} contains unknown {group_name} channels: {unknown_channels}")
        for channel, values in channels.items():
            expected_length = 3 if pattern_type == "wave" else 2
            if len(values) != expected_length:
                raise RuntimeError(
                    f"{name}.{channel} must contain {expected_length} channel values"
                )
            amplitude, frequency = (float(value) for value in values[:2])
            if not math.isfinite(amplitude) or not math.isfinite(frequency):
                raise RuntimeError(f"{name}.{channel} contains a non-finite value")
            if amplitude < 0.0 or frequency < 0.0:
                raise RuntimeError(f"{name}.{channel} amplitude and frequency must be non-negative")
            if pattern_type == "wave" and values[2] not in WAVE_OFFSET_TYPES:
                raise RuntimeError(f"{name}.{channel} has an invalid initial offset type")

    if "fov" in preset:
        values = preset["fov"]
        expected_length = 3 if pattern_type == "wave" else 2
        if len(values) != expected_length:
            raise RuntimeError(f"{name}.fov must contain {expected_length} channel values")
        amplitude, frequency = (float(value) for value in values[:2])
        if (not math.isfinite(amplitude) or not math.isfinite(frequency)
                or amplitude < 0.0 or frequency < 0.0):
            raise RuntimeError(f"{name}.fov amplitude and frequency must be finite and non-negative")
        if pattern_type == "wave" and values[2] not in WAVE_OFFSET_TYPES:
            raise RuntimeError(f"{name}.fov has an invalid initial offset type")


def _create_camera_shake(asset_tools, directory, preset):
    name = preset["name"]
    path = _asset_path(directory, name)
    pattern_type = _get_pattern_type(preset)
    expected_pattern_class = _get_pattern_class(pattern_type)
    if not FORCE_REBUILD and unreal.EditorAssetLibrary.does_asset_exist(path):
        existing_blueprint = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(existing_blueprint, unreal.Blueprint):
            raise RuntimeError(
                f"{path} is not a Blueprint; use Force only if replacement is intentional"
            )
        try:
            _validate_camera_shake(existing_blueprint, path, preset)
        except RuntimeError as exc:
            unreal.log_warning(f"Rebuilding {name}: {exc}")
        else:
            unreal.log(f"Unchanged {name}: CameraShake settings match preset")
            return existing_blueprint

    blueprint = None
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        existing_blueprint = unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(existing_blueprint, unreal.Blueprint):
            try:
                existing_cdo = _get_default_object(_get_generated_class(existing_blueprint, path))
                existing_root = _find_root_pattern(existing_cdo)
            except RuntimeError:
                pass
            else:
                if isinstance(existing_root, expected_pattern_class):
                    blueprint = existing_blueprint

    if blueprint is None:
        if unreal.EditorAssetLibrary.does_asset_exist(path) and not FORCE_REBUILD:
            raise RuntimeError(
                f"{path} is not a compatible {pattern_type} CameraShake Blueprint; "
                "use Force only if replacement is intentional"
            )
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
    if not isinstance(root_pattern, expected_pattern_class):
        root_pattern = unreal.new_object(expected_pattern_class.static_class(), outer=cdo)
        cdo.set_root_shake_pattern(root_pattern)

    if pattern_type == "perlin":
        _configure_perlin_pattern(root_pattern, preset)
    else:
        _configure_wave_pattern(root_pattern, preset)
    _compile_blueprint(blueprint)

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        raise RuntimeError(f"Failed to save CameraShake Blueprint {path}")

    reloaded = unreal.EditorAssetLibrary.load_asset(path)
    reloaded_root = _validate_camera_shake(reloaded, path, preset)
    duration = reloaded_root.get_editor_property("duration")

    unreal.log(f"Generated {name}: {pattern_type} CameraShake duration {duration:.3f}s")
    return reloaded


def _validate_preset_manifest(presets):
    required_sections = (
        "curve_vector_path",
        "camera_shake_path",
        "curve_float_presets",
        "scale_curves",
        "camera_shakes",
    )
    missing_sections = [section for section in required_sections if section not in presets]
    if missing_sections:
        raise RuntimeError(f"Preset manifest is missing sections: {missing_sections}")

    curve_path = _validate_asset_directory(presets["curve_vector_path"])
    shake_path = _validate_asset_directory(presets["camera_shake_path"])
    asset_paths = set()

    def register_asset(directory, name):
        path = _validate_asset_identity(directory, name)
        if path in asset_paths:
            raise RuntimeError(f"Duplicate preset asset path: {path}")
        asset_paths.add(path)

    for preset in presets["curve_float_presets"]:
        _validate_curve_float_preset(preset)
        register_asset(preset["path"], preset["name"])

    for preset in presets["scale_curves"]:
        _validate_curve_vector_preset(preset)
        register_asset(curve_path, preset["name"])

    for preset in presets["camera_shakes"]:
        _validate_camera_shake_preset(preset)
        register_asset(shake_path, preset["name"])

    return curve_path, shake_path


def main():
    presets = _load_presets()
    curve_path, shake_path = _validate_preset_manifest(presets)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    os.makedirs(SOURCE_DIR, exist_ok=True)

    _make_directory(curve_path)
    _make_directory(shake_path)

    if VALIDATE_ONLY:
        for preset in presets["curve_float_presets"]:
            path = _asset_path(preset["path"], preset["name"])
            max_error = _validate_curve_float(unreal.EditorAssetLibrary.load_asset(path), preset)
            unreal.log(f"Validated {preset['name']}: max error {max_error:.7f}")

        for preset in presets["scale_curves"]:
            path = _asset_path(curve_path, preset["name"])
            max_error = _validate_curve_vector(unreal.EditorAssetLibrary.load_asset(path), preset)
            unreal.log(
                f"Validated {preset['name']}: max error {max_error:.7f}, "
                "cubic smoothness passed"
            )

        for preset in presets["camera_shakes"]:
            path = _asset_path(shake_path, preset["name"])
            blueprint = unreal.EditorAssetLibrary.load_asset(path)
            _validate_camera_shake(blueprint, path, preset)

        unreal.log(
            "Validated preset library assets: {0} CurveFloat assets, "
            "{1} CurveVector assets, {2} CameraShake assets".format(
                len(presets["curve_float_presets"]),
                len(presets["scale_curves"]),
                len(presets["camera_shakes"]),
            )
        )
        return

    generated_float_curves = []
    for preset in presets["curve_float_presets"]:
        generated_float_curves.append(_create_curve_float(asset_tools, preset))

    generated_curves = []
    for preset in presets["scale_curves"]:
        generated_curves.append(_create_curve_vector(asset_tools, curve_path, preset))

    generated_shakes = []
    for preset in presets["camera_shakes"]:
        generated_shakes.append(_create_camera_shake(asset_tools, shake_path, preset))

    unreal.log(
        "Generated preset library assets: {0} CurveFloat assets, "
        "{1} CurveVector assets, {2} CameraShake assets".format(
            len(generated_float_curves), len(generated_curves), len(generated_shakes)
        )
    )


if __name__ == "__main__":
    main()
