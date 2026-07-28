"""Generate easings.net-compatible CurveFloat assets in /XTools/Curve/EasingFunctions.

Run with UE 5.3 or later, for example:
UnrealEditor.exe <Project>.uproject -ExecutePythonScript=<ThisFile>

The Python Editor Script Plugin is required only when regenerating the assets.

The mathematical definitions follow https://easings.net/. Smooth intervals use
adaptive cubic Hermite keys. Intentional Bounce impacts and the vertical
tangents in circular easing remain explicit interval boundaries.

The easings.net endpoint branches introduce sub-0.001 jumps in Expo and Elastic
functions. These asset versions normalize the underlying formulas so Unreal
curves remain continuous while still reaching exactly 0 and 1.
"""

import math
import os

import unreal


ASSET_PATH = "/XTools/Curve/EasingFunctions"
VALIDATE_ONLY = os.environ.get("XTOOLS_EASING_CURVE_VALIDATE_ONLY") == "1"
VALIDATION_STEPS = 131072
ADAPTIVE_TEST_SAMPLES = 32
MAX_SUBDIVISION_DEPTH = 24
MAX_VERTICAL_ERROR = 0.00025
ADAPTIVE_ERROR_LIMIT = MAX_VERTICAL_ERROR * 0.75
CIRCULAR_REGULARIZATION = 0.0005


def _normalize_unit_interval(function):
    start_value = function(0.0)
    value_range = function(1.0) - start_value
    return lambda x: (function(x) - start_value) / value_range


def _bounce_out(x):
    n1 = 7.5625
    d1 = 2.75
    if x < 1.0 / d1:
        return n1 * x * x
    if x < 2.0 / d1:
        x -= 1.5 / d1
        return n1 * x * x + 0.75
    if x < 2.5 / d1:
        x -= 2.25 / d1
        return n1 * x * x + 0.9375
    x -= 2.625 / d1
    return n1 * x * x + 0.984375


def _ease_in_out_power(x, power):
    if x < 0.5:
        return (2.0 ** (power - 1)) * (x ** power)
    return 1.0 - ((-2.0 * x + 2.0) ** power) / 2.0


def _raw_ease_in_expo(x):
    return 2.0 ** (10.0 * x - 10.0)


def _raw_ease_out_expo(x):
    return 1.0 - 2.0 ** (-10.0 * x)


def _raw_ease_in_out_expo(x):
    if x < 0.5:
        return (2.0 ** (20.0 * x - 10.0)) / 2.0
    return (2.0 - 2.0 ** (-20.0 * x + 10.0)) / 2.0


def _stable_circ_in(x):
    # The exact circular easing has an infinite endpoint tangent. That creates
    # near-overlapping keys and very large tangents that can hang UE's Curve Editor.
    epsilon = CIRCULAR_REGULARIZATION
    numerator = math.sqrt(1.0 + epsilon) - math.sqrt(1.0 + epsilon - x * x)
    denominator = math.sqrt(1.0 + epsilon) - math.sqrt(epsilon)
    return numerator / denominator


def _stable_circ_out(x):
    return 1.0 - _stable_circ_in(1.0 - x)


def _stable_circ_in_out(x):
    if x < 0.5:
        return _stable_circ_in(2.0 * x) * 0.5
    return 1.0 - _stable_circ_in(2.0 - 2.0 * x) * 0.5


def _ease_in_back(x):
    c1 = 1.70158
    return (c1 + 1.0) * x * x * x - c1 * x * x


def _ease_out_back(x):
    c1 = 1.70158
    return 1.0 + (c1 + 1.0) * (x - 1.0) ** 3 + c1 * (x - 1.0) ** 2


def _ease_in_out_back(x):
    c2 = 1.70158 * 1.525
    if x < 0.5:
        return ((2.0 * x) ** 2 * ((c2 + 1.0) * 2.0 * x - c2)) / 2.0
    return (((2.0 * x - 2.0) ** 2) * ((c2 + 1.0) * (2.0 * x - 2.0) + c2) + 2.0) / 2.0


def _raw_ease_in_elastic(x):
    c4 = 2.0 * math.pi / 3.0
    return -(2.0 ** (10.0 * x - 10.0)) * math.sin((10.0 * x - 10.75) * c4)


def _raw_ease_out_elastic(x):
    c4 = 2.0 * math.pi / 3.0
    return (2.0 ** (-10.0 * x)) * math.sin((10.0 * x - 0.75) * c4) + 1.0


def _raw_ease_in_out_elastic(x):
    c5 = 2.0 * math.pi / 4.5
    if x < 0.5:
        return -((2.0 ** (20.0 * x - 10.0)) * math.sin((20.0 * x - 11.125) * c5)) / 2.0
    return ((2.0 ** (-20.0 * x + 10.0)) * math.sin((20.0 * x - 11.125) * c5)) / 2.0 + 1.0


EASINGS = {
    "EaseInSine": lambda x: 1.0 - math.cos(x * math.pi / 2.0),
    "EaseOutSine": lambda x: math.sin(x * math.pi / 2.0),
    "EaseInOutSine": lambda x: -(math.cos(math.pi * x) - 1.0) / 2.0,
    "EaseInQuad": lambda x: x * x,
    "EaseOutQuad": lambda x: 1.0 - (1.0 - x) ** 2,
    "EaseInOutQuad": lambda x: _ease_in_out_power(x, 2),
    "EaseInCubic": lambda x: x ** 3,
    "EaseOutCubic": lambda x: 1.0 - (1.0 - x) ** 3,
    "EaseInOutCubic": lambda x: _ease_in_out_power(x, 3),
    "EaseInQuart": lambda x: x ** 4,
    "EaseOutQuart": lambda x: 1.0 - (1.0 - x) ** 4,
    "EaseInOutQuart": lambda x: _ease_in_out_power(x, 4),
    "EaseInQuint": lambda x: x ** 5,
    "EaseOutQuint": lambda x: 1.0 - (1.0 - x) ** 5,
    "EaseInOutQuint": lambda x: _ease_in_out_power(x, 5),
    "EaseInExpo": _normalize_unit_interval(_raw_ease_in_expo),
    "EaseOutExpo": _normalize_unit_interval(_raw_ease_out_expo),
    "EaseInOutExpo": _normalize_unit_interval(_raw_ease_in_out_expo),
    "EaseInCirc": _stable_circ_in,
    "EaseOutCirc": _stable_circ_out,
    "EaseInOutCirc": _stable_circ_in_out,
    "EaseInBack": _ease_in_back,
    "EaseOutBack": _ease_out_back,
    "EaseInOutBack": _ease_in_out_back,
    "EaseInElastic": _normalize_unit_interval(_raw_ease_in_elastic),
    "EaseOutElastic": _normalize_unit_interval(_raw_ease_out_elastic),
    "EaseInOutElastic": _normalize_unit_interval(_raw_ease_in_out_elastic),
    "EaseInBounce": lambda x: 1.0 - _bounce_out(1.0 - x),
    "EaseOutBounce": _bounce_out,
    "EaseInOutBounce": lambda x: (
        (1.0 - _bounce_out(1.0 - 2.0 * x)) / 2.0
        if x < 0.5
        else (1.0 + _bounce_out(2.0 * x - 1.0)) / 2.0
    ),
}

ASSET_NAMES = {
    "EaseInSine": "Curve_正弦缓入0-1",
    "EaseOutSine": "Curve_正弦缓出0-1",
    "EaseInOutSine": "Curve_正弦缓入缓出0-1",
    "EaseInQuad": "Curve_二次方缓入0-1",
    "EaseOutQuad": "Curve_二次方缓出0-1",
    "EaseInOutQuad": "Curve_二次方缓入缓出0-1",
    "EaseInCubic": "Curve_三次方缓入0-1",
    "EaseOutCubic": "Curve_三次方缓出0-1",
    "EaseInOutCubic": "Curve_三次方缓入缓出0-1",
    "EaseInQuart": "Curve_四次方缓入0-1",
    "EaseOutQuart": "Curve_四次方缓出0-1",
    "EaseInOutQuart": "Curve_四次方缓入缓出0-1",
    "EaseInQuint": "Curve_五次方缓入0-1",
    "EaseOutQuint": "Curve_五次方缓出0-1",
    "EaseInOutQuint": "Curve_五次方缓入缓出0-1",
    "EaseInExpo": "Curve_指数缓入0-1",
    "EaseOutExpo": "Curve_指数缓出0-1",
    "EaseInOutExpo": "Curve_指数缓入缓出0-1",
    "EaseInCirc": "Curve_圆形缓入0-1",
    "EaseOutCirc": "Curve_圆形缓出0-1",
    "EaseInOutCirc": "Curve_圆形缓入缓出0-1",
    "EaseInBack": "Curve_回退缓入0-1",
    "EaseOutBack": "Curve_回退缓出0-1",
    "EaseInOutBack": "Curve_回退缓入缓出0-1",
    "EaseInElastic": "Curve_弹性缓入0-1",
    "EaseOutElastic": "Curve_弹性缓出0-1",
    "EaseInOutElastic": "Curve_弹性缓入缓出0-1",
    "EaseInBounce": "Curve_回弹缓入0-1",
    "EaseOutBounce": "Curve_回弹缓出0-1",
    "EaseInOutBounce": "Curve_回弹缓入缓出0-1",
}


def _bounce_breakpoints():
    d1 = 2.75
    out_breaks = (1.0 / d1, 2.0 / d1, 2.5 / d1)
    return {
        "EaseOutBounce": [0.0, *out_breaks, 1.0],
        "EaseInBounce": [0.0, *(1.0 - point for point in reversed(out_breaks)), 1.0],
        "EaseInOutBounce": [
            0.0,
            *((1.0 - point) * 0.5 for point in reversed(out_breaks)),
            0.5,
            *((1.0 + point) * 0.5 for point in out_breaks),
            1.0,
        ],
    }


EASING_BREAKPOINTS = {
    **_bounce_breakpoints(),
}


def _derivative(function, time, interval_start, interval_end):
    step = min(1.0e-5, (interval_end - interval_start) / 1000.0)
    if time - interval_start < 2.1 * step:
        return (
            -25.0 * function(time)
            + 48.0 * function(time + step)
            - 36.0 * function(time + 2.0 * step)
            + 16.0 * function(time + 3.0 * step)
            - 3.0 * function(time + 4.0 * step)
        ) / (12.0 * step)
    if interval_end - time < 2.1 * step:
        return (
            25.0 * function(time)
            - 48.0 * function(time - step)
            + 36.0 * function(time - 2.0 * step)
            - 16.0 * function(time - 3.0 * step)
            + 3.0 * function(time - 4.0 * step)
        ) / (12.0 * step)
    return (
        function(time - 2.0 * step)
        - 8.0 * function(time - step)
        + 8.0 * function(time + step)
        - function(time + 2.0 * step)
    ) / (12.0 * step)


def _evaluate_hermite(segment, time):
    start, end, start_value, end_value, start_tangent, end_tangent = segment
    alpha = (time - start) / (end - start)
    alpha_squared = alpha * alpha
    alpha_cubed = alpha_squared * alpha
    duration = end - start
    return (
        (2.0 * alpha_cubed - 3.0 * alpha_squared + 1.0) * start_value
        + (alpha_cubed - 2.0 * alpha_squared + alpha) * duration * start_tangent
        + (-2.0 * alpha_cubed + 3.0 * alpha_squared) * end_value
        + (alpha_cubed - alpha_squared) * duration * end_tangent
    )


def _build_cubic_segments(easing_name, function):
    segments = []

    def append_adaptive(start, end, interval_start, interval_end, depth):
        segment = (
            start,
            end,
            function(start),
            function(end),
            _derivative(function, start, interval_start, interval_end),
            _derivative(function, end, interval_start, interval_end),
        )
        max_error = max(
            abs(
                _evaluate_hermite(segment, start + (end - start) * index / (ADAPTIVE_TEST_SAMPLES + 1))
                - function(start + (end - start) * index / (ADAPTIVE_TEST_SAMPLES + 1))
            )
            for index in range(1, ADAPTIVE_TEST_SAMPLES + 1)
        )
        if max_error <= ADAPTIVE_ERROR_LIMIT:
            segments.append(segment)
            return
        if depth >= MAX_SUBDIVISION_DEPTH:
            raise RuntimeError(
                f"{easing_name} could not meet error limit in [{start}, {end}]: {max_error}"
            )

        midpoint = (start + end) * 0.5
        append_adaptive(start, midpoint, interval_start, interval_end, depth + 1)
        append_adaptive(midpoint, end, interval_start, interval_end, depth + 1)

    breakpoints = EASING_BREAKPOINTS.get(easing_name, [0.0, 1.0])
    for interval_start, interval_end in zip(breakpoints, breakpoints[1:]):
        append_adaptive(interval_start, interval_end, interval_start, interval_end, 0)
    return segments


def _segments_to_keys(segments):
    keys = []
    for segment in segments:
        start, end, start_value, end_value, start_tangent, end_tangent = segment
        if not keys:
            keys.append([start, start_value, start_tangent, start_tangent])
        else:
            keys[-1][3] = start_tangent
        keys.append([end, end_value, end_tangent, end_tangent])
    return keys


def _build_cubic_keys(easing_name, function):
    return _segments_to_keys(_build_cubic_segments(easing_name, function))


def _validate_key_smoothness(easing_name, keys):
    allowed_corners = EASING_BREAKPOINTS.get(easing_name, [0.0, 1.0])[1:-1]
    corner_count = 0
    for time, value, arrive_tangent, leave_tangent in keys:
        if not all(math.isfinite(item) for item in (time, value, arrive_tangent, leave_tangent)):
            raise RuntimeError(f"{easing_name} contains a non-finite cubic key")
        if abs(arrive_tangent - leave_tangent) <= 1.0e-5:
            continue
        if not any(abs(time - allowed_time) <= 1.0e-9 for allowed_time in allowed_corners):
            raise RuntimeError(f"{easing_name} has an unintended tangent break at {time}")
        corner_count += 1
    return corner_count


def _import_curve(asset_tools, asset_name, easing_name, keys, source_directory):
    source_filename = os.path.join(source_directory, f"{easing_name}.csv")
    with open(source_filename, "w", encoding="utf-8", newline="") as source_file:
        for time, value, _arrive_tangent, _leave_tangent in keys:
            source_file.write(f"{time:.9g},{value:.9g}\n")

    settings = unreal.CSVImportSettings()
    settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_CURVE_FLOAT)
    factory = unreal.CSVImportFactory()
    factory.set_editor_property("automated_import_settings", settings)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_filename)
    task.set_editor_property("destination_path", ASSET_PATH)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", factory)

    asset_tools.import_asset_tasks([task])
    imported_objects = task.get_objects()
    if len(imported_objects) != 1 or not isinstance(imported_objects[0], unreal.CurveFloat):
        raise RuntimeError(f"Failed to import {ASSET_PATH}/{asset_name}")
    asset = imported_objects[0]
    key_data = [unreal.Vector4(x=time, y=value, z=arrive, w=leave) for time, value, arrive, leave in keys]
    if not unreal.XCurvePresetLibrary.set_curve_float_cubic_keys(asset, key_data):
        raise RuntimeError(f"Failed to set cubic keys on {ASSET_PATH}/{asset_name}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save {ASSET_PATH}/{asset_name}")
    return asset


def _validate_curve(asset, asset_name, function):
    if not isinstance(asset, unreal.CurveFloat):
        raise RuntimeError(f"{asset_name} is not a CurveFloat")

    min_time, max_time = asset.get_time_range()
    if abs(min_time) > 1.0e-9 or abs(max_time - 1.0) > 1.0e-9:
        raise RuntimeError(f"{asset_name} has invalid time range [{min_time}, {max_time}]")

    max_error = 0.0
    for index in range(VALIDATION_STEPS + 1):
        time = index / VALIDATION_STEPS
        max_error = max(max_error, abs(asset.get_float_value(time) - function(time)))
    if max_error > MAX_VERTICAL_ERROR + 1.0e-6:
        raise RuntimeError(f"{asset_name} exceeds error limit: {max_error}")
    return max_error


def main():
    if len(EASINGS) != 30 or set(EASINGS) != set(ASSET_NAMES):
        raise RuntimeError(f"Expected 30 easing functions, found {len(EASINGS)}")

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    source_directory = os.path.abspath(
        os.path.join(unreal.Paths.project_saved_dir(), "XTools", "EasingCurveImport")
    )
    os.makedirs(source_directory, exist_ok=True)
    total_keys = 0
    for easing_name, function in EASINGS.items():
        if abs(function(0.0)) > 1.0e-9 or abs(function(1.0) - 1.0) > 1.0e-9:
            raise RuntimeError(f"{easing_name} has invalid endpoints")

        asset_name = ASSET_NAMES[easing_name]
        keys = _build_cubic_keys(easing_name, function)
        corner_count = _validate_key_smoothness(easing_name, keys)
        if VALIDATE_ONLY:
            asset = unreal.load_asset(f"{ASSET_PATH}/{asset_name}", unreal.CurveFloat)
        else:
            asset = _import_curve(asset_tools, asset_name, easing_name, keys, source_directory)
        total_keys += len(keys)
        max_error = _validate_curve(asset, asset_name, function)

        unreal.log(
            f"{'Validated' if VALIDATE_ONLY else 'Generated'} {asset_name}: "
            f"{len(keys)} cubic keys, {corner_count} intentional corners, max error {max_error:.7f}"
        )

    unreal.log(
        f"{'Validated' if VALIDATE_ONLY else 'Generated'} {len(EASINGS)} easing CurveFloat assets "
        f"with {total_keys} total cubic keys"
    )


if __name__ == "__main__":
    main()
