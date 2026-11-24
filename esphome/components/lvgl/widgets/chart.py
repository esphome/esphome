from collections.abc import Callable
import math
from typing import Any

from esphome import automation, codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR,
    CONF_HIDDEN,
    CONF_ID,
    CONF_INDEX,
    CONF_ITEMS,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_TYPE,
    CONF_VALUE,
    CONF_X,
    CONF_Y,
)
from esphome.core import ID, Lambda
from esphome.cpp_generator import MockObj
from esphome.final_validate import FinalValidateConfig, full_config
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

from ..automation import action_to_code
from ..defines import (
    CONF_INDICATOR,
    CONF_MAIN,
    CONF_POINTS,
    LV_CHART_TYPES,
    LValidator,
    LvConstant,
    get_chart_series,
    get_chart_series_actions,
    get_updated_widgets,
    literal,
)
from ..lv_validation import lv_bool, lv_color, lv_float, lv_positive_int
from ..lvcode import LocalVariable, lv, lv_assign, lv_expr, lv_global
from ..types import LvType, lvgl_ns
from . import Widget, WidgetType

CONF_CHART = "chart"
ACTION_APPEND = "lvgl.chart.series.append"
ACTION_SET = "lvgl.chart.series.set"
ACTION_CLEAR = "lvgl.chart.series.clear"
ACTION_UPDATE = "lvgl.chart.series.update"
CONF_GRID_LINES = "grid_lines"
CONF_MULTIPLIER = "multiplier"
CONF_POINT_COUNT = "point_count"
CONF_POINT = "point"
CONF_SECONDARY_X_AXIS = "secondary_x_axis"
CONF_SECONDARY_Y_AXIS = "secondary_y_axis"
CONF_SERIES = "series"
CONF_UPDATE_MODE = "update_mode"
CONF_VALUES = "values"
CONF_X_AXIS = "x_axis"
CONF_Y_AXIS = "y_axis"

CHART_TYPES = LvConstant("LV_CHART_TYPE_", *LV_CHART_TYPES)
CHART_UPDATE_MODES = LvConstant("LV_CHART_UPDATE_MODE_", "SHIFT", "CIRCULAR")
# Colours given in turn to series that do not set one
SERIES_PALETTE = ("BLUE", "RED", "GREEN", "ORANGE", "PURPLE", "TEAL")
GRID_LINES = cv.int_range(min=0, max=255)
SECONDARY = "secondary"
# Which of the chart's axes a series is drawn against
SERIES_AXIS = cv.one_of("primary", SECONDARY, lower=True)


def _fixed(constant: LvConstant) -> Callable[[Any], str]:
    """Accept one of the constant's values, but not a lambda."""

    @schema_extractor("one_of")
    def validator(value: Any) -> str:
        if value == SCHEMA_EXTRACT:
            return constant.choices
        if isinstance(value, Lambda):
            raise cv.Invalid("This option cannot be a lambda")
        return constant.one_of(value)

    return validator


CHART_TYPE = _fixed(CHART_TYPES)
CHART_UPDATE_MODE = _fixed(CHART_UPDATE_MODES)

lv_chart_t = LvType("lv_chart_t")
lv_chart_series_t = cg.global_ns.struct("lv_chart_series_t")
chart_point_value = lvgl_ns.chart_point_value
chart_point_slot = lvgl_ns.chart_point_slot

# The largest float below 2**31; matches the limit in chart_point_value(). INT32_MAX
# itself is LV_CHART_POINT_NONE, which marks a gap.
POINT_LIMIT = 2147483520


def _finite(value: float) -> float:
    if not math.isfinite(value):
        raise cv.Invalid("Must be a finite number")
    return value


lv_finite_float = LValidator(cv.All(cv.float_, _finite), cg.float_)


def scale_value(value: float, multiplier: float) -> int | None:
    """A fixed chart value as LVGL stores it, as chart_point_value() does at run
    time: scaled, rounded half away from zero and limited to the int32 range.
    None for NaN, which leaves a gap."""
    if math.isnan(value):
        return None
    scaled = max(-POINT_LIMIT, min(POINT_LIMIT, value * multiplier))
    return int(math.copysign(math.floor(abs(scaled) + 0.5), scaled))


def _validate_range(config: ConfigType, multiplier: float | None = None) -> None:
    """LVGL divides by the width of the range when it draws a bar chart."""
    if CONF_MIN_VALUE not in config:
        return
    min_value = config[CONF_MIN_VALUE]
    max_value = config[CONF_MAX_VALUE]
    if isinstance(min_value, Lambda) or isinstance(max_value, Lambda):
        return
    if multiplier is None:
        multiplier = config.get(CONF_MULTIPLIER, 1.0)
    if scale_value(min_value, multiplier) == scale_value(max_value, multiplier):
        raise cv.Invalid(
            f"{CONF_MIN_VALUE} and {CONF_MAX_VALUE} must not be equal "
            f"after scaling by {CONF_MULTIPLIER} ({multiplier:g})"
        )


def _validate_created_range(config: ConfigType) -> ConfigType:
    _validate_range(config)
    return config


def _validate_updated_range(config: ConfigType) -> ConfigType:
    # The multiplier is set when the chart is created; ChartType.final_validate()
    # checks the range scaled by it. Equal values are wrong whatever it is.
    _validate_range(config, 1.0)
    return config


def _axis_schema(grid_lines: bool) -> cv.All:
    """An axis of a new chart. Only the primary axes have grid lines, since the
    secondary axes share the same grid. Without a range, LVGL uses 0 to 100."""
    schema = {
        cv.Inclusive(CONF_MIN_VALUE, "range"): lv_finite_float,
        cv.Inclusive(CONF_MAX_VALUE, "range"): lv_finite_float,
        cv.Optional(CONF_MULTIPLIER): cv.All(cv.positive_not_null_float, _finite),
    }
    if grid_lines:
        schema[cv.Optional(CONF_GRID_LINES)] = GRID_LINES
    return cv.All(cv.Schema(schema), _validate_created_range)


def _axis_modify_schema(grid_lines: bool) -> cv.All:
    """An axis in lvgl.chart.update: a new range, new grid lines, or both."""
    if not grid_lines:
        return cv.All(
            cv.Schema(
                {
                    cv.Required(CONF_MIN_VALUE): lv_finite_float,
                    cv.Required(CONF_MAX_VALUE): lv_finite_float,
                }
            ),
            _validate_updated_range,
        )
    return cv.All(
        cv.Schema(
            {
                cv.Inclusive(CONF_MIN_VALUE, "range"): lv_finite_float,
                cv.Inclusive(CONF_MAX_VALUE, "range"): lv_finite_float,
                cv.Optional(CONF_GRID_LINES): GRID_LINES,
            }
        ),
        cv.has_at_least_one_key(CONF_MIN_VALUE, CONF_GRID_LINES),
        _validate_updated_range,
    )


POINT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_X): lv_float,
        cv.Required(CONF_Y): lv_float,
    }
)

SERIES_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(lv_chart_series_t),
        cv.Optional(CONF_COLOR): lv_color,
        cv.Optional(CONF_HIDDEN, default=False): cv.boolean,
        cv.Optional(CONF_Y_AXIS, default="primary"): SERIES_AXIS,
        cv.Optional(CONF_X_AXIS, default="primary"): SERIES_AXIS,
        cv.Optional(CONF_VALUES): cv.ensure_list(lv_float),
        cv.Optional(CONF_POINTS): cv.ensure_list(POINT_SCHEMA),
    }
)


def _record(action: str):
    """Remember the action so validate_chart_series_actions() can check it against
    its chart once the whole config is known."""

    def validator(config: ConfigType) -> ConfigType:
        get_chart_series_actions().append((action, config))
        return config

    return validator


def _series_schema(action: str, schema: dict, *validators) -> cv.All:
    return cv.All(
        cv.Schema({cv.Required(CONF_ID): cv.use_id(lv_chart_series_t), **schema}),
        *validators,
        _record(action),
    )


def _index_goes_with_one_point(config: ConfigType) -> ConfigType:
    one_point = CONF_VALUE in config or CONF_POINT in config
    if one_point and CONF_INDEX not in config:
        raise cv.Invalid(f"{CONF_INDEX} is required to set a single point")
    if not one_point and CONF_INDEX in config:
        raise cv.Invalid(
            f"{CONF_INDEX} can only be used with {CONF_VALUE} or {CONF_POINT}",
            path=[CONF_INDEX],
        )
    return config


SERIES_APPEND_SCHEMA = _series_schema(
    ACTION_APPEND,
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_POINT): POINT_SCHEMA,
    },
    cv.has_exactly_one_key(CONF_VALUE, CONF_POINT),
)

SERIES_SET_SCHEMA = _series_schema(
    ACTION_SET,
    {
        cv.Optional(CONF_VALUES): cv.ensure_list(lv_float),
        cv.Optional(CONF_POINTS): cv.ensure_list(POINT_SCHEMA),
        cv.Optional(CONF_INDEX): lv_positive_int,
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_POINT): POINT_SCHEMA,
    },
    cv.has_exactly_one_key(CONF_VALUES, CONF_POINTS, CONF_VALUE, CONF_POINT),
    _index_goes_with_one_point,
)

SERIES_CLEAR_SCHEMA = _series_schema(ACTION_CLEAR, {})

SERIES_UPDATE_SCHEMA = _series_schema(
    ACTION_UPDATE,
    {
        cv.Optional(CONF_COLOR): lv_color,
        cv.Optional(CONF_HIDDEN): lv_bool,
    },
    cv.has_at_least_one_key(CONF_COLOR, CONF_HIDDEN),
)


CHART_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TYPE, default="LINE"): CHART_TYPE,
        cv.Optional(CONF_UPDATE_MODE, default="SHIFT"): CHART_UPDATE_MODE,
        cv.Optional(CONF_POINT_COUNT, default=10): cv.int_range(min=1, max=4096),
        cv.Optional(CONF_X_AXIS): _axis_schema(grid_lines=True),
        cv.Optional(CONF_Y_AXIS): _axis_schema(grid_lines=True),
        cv.Optional(CONF_SECONDARY_X_AXIS): _axis_schema(grid_lines=False),
        cv.Optional(CONF_SECONDARY_Y_AXIS): _axis_schema(grid_lines=False),
        cv.Optional(CONF_SERIES): cv.ensure_list(SERIES_SCHEMA),
    }
)

CHART_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_UPDATE_MODE): CHART_UPDATE_MODE,
        cv.Optional(CONF_POINT_COUNT): cv.int_range(min=1, max=4096),
        cv.Optional(CONF_X_AXIS): _axis_modify_schema(grid_lines=True),
        cv.Optional(CONF_Y_AXIS): _axis_modify_schema(grid_lines=True),
        cv.Optional(CONF_SECONDARY_X_AXIS): _axis_modify_schema(grid_lines=False),
        cv.Optional(CONF_SECONDARY_Y_AXIS): _axis_modify_schema(grid_lines=False),
    }
)


def x_axis_error(chart_type: str, config: ConfigType) -> cv.Invalid | None:
    """Only a scatter chart places points by their X values, so only it has an X range
    or a secondary X axis; other charts space their values evenly."""
    if chart_type == "LV_CHART_TYPE_SCATTER":
        return None
    if CONF_SECONDARY_X_AXIS in config:
        return cv.Invalid(
            "Only a scatter chart has a secondary X axis", path=[CONF_SECONDARY_X_AXIS]
        )
    x_axis = config.get(CONF_X_AXIS, {})
    for key in (CONF_MIN_VALUE, CONF_MULTIPLIER):
        if key in x_axis:
            return cv.Invalid(
                "Only a scatter chart has an X range; other charts space their values "
                f"evenly, and {CONF_X_AXIS} can only set {CONF_GRID_LINES}",
                path=[CONF_X_AXIS, key],
            )
    return None


def _negative_stacked_value(chart_type: str, values: list | None) -> bool:
    """LVGL stacks bars only for values of 0 or more."""
    return chart_type == "LV_CHART_TYPE_STACKED" and any(
        isinstance(value, (int, float)) and value < 0 for value in values or ()
    )


STACKED_NEGATIVE = "A stacked chart can't show negative values"


def validate_chart(config: ConfigType) -> ConfigType:
    point_count = config[CONF_POINT_COUNT]
    is_scatter = config[CONF_TYPE] == "LV_CHART_TYPE_SCATTER"
    if (error := x_axis_error(config[CONF_TYPE], config)) is not None:
        raise error
    for index, series in enumerate(config.get(CONF_SERIES, ())):
        values = series.get(CONF_VALUES)
        points = series.get(CONF_POINTS)
        if is_scatter and values is not None:
            raise cv.Invalid(
                "A scatter chart series must use points instead of values",
                path=[CONF_SERIES, index, CONF_VALUES],
            )
        if not is_scatter and points is not None:
            raise cv.Invalid(
                "points can only be used with a scatter chart",
                path=[CONF_SERIES, index, CONF_POINTS],
            )
        if not is_scatter and series[CONF_X_AXIS] == SECONDARY:
            raise cv.Invalid(
                "Only a scatter chart series can use the secondary X axis",
                path=[CONF_SERIES, index, CONF_X_AXIS],
            )
        if _negative_stacked_value(config[CONF_TYPE], values):
            raise cv.Invalid(STACKED_NEGATIVE, path=[CONF_SERIES, index, CONF_VALUES])
        key, data = (CONF_POINTS, points) if is_scatter else (CONF_VALUES, values)
        if data is not None and len(data) > point_count:
            raise cv.Invalid(
                f"A chart series can't have more than point_count ({point_count}) entries",
                path=[CONF_SERIES, index, key],
            )
    return config


def validate_series_action(
    action: str, config: ConfigType, chart_type: str, point_count: int
) -> None:
    """Check a series action's data against its chart's type and point count."""
    is_scatter = chart_type == "LV_CHART_TYPE_SCATTER"
    # The scatter key and the key for other charts, for one point and for all of them
    data_keys = ((CONF_POINT, CONF_VALUE), (CONF_POINTS, CONF_VALUES))
    for scatter_key, other_key in data_keys:
        if is_scatter and other_key in config:
            raise cv.Invalid(
                f"A scatter chart series takes {scatter_key}, not {other_key}",
                path=[other_key],
            )
        if not is_scatter and scatter_key in config:
            raise cv.Invalid(
                f"Only a scatter chart series takes {scatter_key}; use {other_key}",
                path=[scatter_key],
            )
    for key in (CONF_VALUES, CONF_POINTS):
        if (data := config.get(key)) is not None and len(data) > point_count:
            raise cv.Invalid(
                f"{key} can't have more than point_count ({point_count}) entries",
                path=[key],
            )
    for key in (CONF_VALUE, CONF_VALUES):
        values = config.get(key)
        if _negative_stacked_value(
            chart_type, values if isinstance(values, list) else [values]
        ):
            raise cv.Invalid(STACKED_NEGATIVE, path=[key])
    index = config.get(CONF_INDEX)
    if isinstance(index, int) and index >= point_count:
        raise cv.Invalid(
            f"{CONF_INDEX} must be less than point_count ({point_count})",
            path=[CONF_INDEX],
        )


def _smallest_point_counts(global_config: FinalValidateConfig) -> dict[tuple, int]:
    """Map each chart path to the smallest point_count that lvgl.chart.update can give it."""
    smallest: dict[tuple, int] = {}
    for update in get_updated_widgets().get(chart_spec, ()):
        if (point_count := update.get(CONF_POINT_COUNT)) is None:
            continue
        for id_conf in update[CONF_ID]:
            path = tuple(global_config.get_path_for_id(id_conf[CONF_ID])[:-1])
            smallest[path] = min(smallest.get(path, point_count), point_count)
    return smallest


def _find_path(node: Any, target: ConfigType, path: list) -> list | None:
    """The path to the object `target` (compared by identity) within `node`."""
    if node is target:
        return path
    if isinstance(node, dict):
        children = node.items()
    elif isinstance(node, list):
        children = enumerate(node)
    else:
        return None
    for key, child in children:
        if (found := _find_path(child, target, [*path, key])) is not None:
            return found
    return None


def validate_chart_series_actions(global_config: FinalValidateConfig) -> None:
    """Check each lvgl.chart.series.* action against the chart its series belongs to."""
    smallest_point_counts = _smallest_point_counts(global_config)
    errors = []
    for action, config in get_chart_series_actions():
        series_id = config[CONF_ID]
        # A series ID is declared at <chart>/series/<index>/id
        chart_path = tuple(global_config.get_path_for_id(series_id)[:-3])
        chart_config = global_config.get_config_for_path(list(chart_path))
        point_count = min(
            chart_config[CONF_POINT_COUNT],
            smallest_point_counts.get(chart_path, chart_config[CONF_POINT_COUNT]),
        )
        try:
            validate_series_action(action, config, chart_config[CONF_TYPE], point_count)
        except cv.Invalid as err:
            # The action may sit outside the lvgl block, so give its full path; the
            # root marker stops the lvgl block's path being put in front of it.
            action_path = _find_path(global_config, config, [])
            path = (
                []
                if action_path is None
                else [cv.ROOT_CONFIG_PATH, *action_path, *err.path]
            )
            errors.append(
                cv.Invalid(f"'{action}' for series '{series_id}': {err.msg}", path=path)
            )
    if errors:
        raise cv.MultipleInvalid(errors)


class ChartType(WidgetType):
    def __init__(self) -> None:
        super().__init__(
            CONF_CHART,
            lv_chart_t,
            (CONF_MAIN, CONF_ITEMS, CONF_INDICATOR),
            CHART_SCHEMA.add_extra(validate_chart),
            CHART_MODIFY_SCHEMA,
        )

    def final_validate(
        self,
        widget: Any,
        update_config: ConfigType,
        widget_config: ConfigType,
        path: list,
    ) -> None:
        if (error := x_axis_error(widget_config[CONF_TYPE], update_config)) is not None:
            action_path = _find_path(full_config.get(), update_config, [])
            raise cv.Invalid(
                f"'lvgl.chart.update' for '{widget}': {error.msg}",
                path=[]
                if action_path is None
                else [cv.ROOT_CONFIG_PATH, *action_path, *error.path],
            )
        for axis_name, _ in AXES:
            if (axis := update_config.get(axis_name)) is None or (
                CONF_MIN_VALUE not in axis
            ):
                continue
            try:
                _validate_range(axis, axis_multiplier(widget_config, axis_name))
            except cv.Invalid as err:
                action_path = _find_path(full_config.get(), update_config, [])
                raise cv.Invalid(
                    f"'lvgl.chart.update' for '{widget}': {err.msg}",
                    path=[]
                    if action_path is None
                    else [cv.ROOT_CONFIG_PATH, *action_path, axis_name],
                ) from err

    async def create_to_code(self, config: ConfigType, parent: MockObj) -> Widget:
        # Options and series are set here rather than in to_code(), which
        # lvgl.widget.refresh runs again: it must not declare the series a second time
        # or undo what lvgl.chart.update changed. super() runs to_code(), which sets
        # the templated axis ranges.
        w = await super().create_to_code(config, parent)
        await set_chart_options(w.obj, config, config, templated=False)
        for index, series in enumerate(config.get(CONF_SERIES, ())):
            # Recorded before the variable exists, so an action waiting on it can find the chart.
            get_chart_series()[str(series[CONF_ID])] = (w, series)
            series_var = cg.Pvariable(
                series[CONF_ID], cg.nullptr, type_=lv_chart_series_t
            )
            lv_assign(
                series_var,
                lv_expr.chart_add_series(
                    w.obj,
                    await series_color(series, index),
                    series_axes(series),
                ),
            )
            if series[CONF_HIDDEN]:
                lv.chart_hide_series(w.obj, series_var, True)
            x_scale, y_scale = series_multipliers(config, series)
            if config[CONF_TYPE] == "LV_CHART_TYPE_SCATTER":
                await set_series_points(
                    w.obj, series_var, series.get(CONF_POINTS, ()), x_scale, y_scale
                )
            else:
                await set_series_values(
                    w.obj, series_var, series.get(CONF_VALUES, ()), y_scale
                )

        lv.chart_refresh(w.obj)
        return w

    async def update_to_code(self, w: Widget, config: ConfigType) -> None:
        await set_chart_options(w.obj, config, w.config)
        lv.chart_refresh(w.obj)

    async def to_code(self, w: Widget, config: ConfigType) -> None:
        await set_axis_ranges(w.obj, config, w.config, templated=True)
        lv.chart_refresh(w.obj)


AXES = (
    (CONF_Y_AXIS, "PRIMARY_Y"),
    (CONF_SECONDARY_Y_AXIS, "SECONDARY_Y"),
    (CONF_X_AXIS, "PRIMARY_X"),
    (CONF_SECONDARY_X_AXIS, "SECONDARY_X"),
)


def _is_templated(axis: ConfigType) -> bool:
    return isinstance(axis[CONF_MIN_VALUE], Lambda) or isinstance(
        axis[CONF_MAX_VALUE], Lambda
    )


def axis_multiplier(chart_config: ConfigType, axis_name: str) -> float:
    """The multiplier the chart was created with for an axis."""
    return chart_config.get(axis_name, {}).get(CONF_MULTIPLIER, 1.0)


def series_multipliers(
    chart_config: ConfigType, series: ConfigType
) -> tuple[float, float]:
    """The X and Y multipliers of the axes a series is drawn against."""
    x_axis = CONF_SECONDARY_X_AXIS if series[CONF_X_AXIS] == SECONDARY else CONF_X_AXIS
    y_axis = CONF_SECONDARY_Y_AXIS if series[CONF_Y_AXIS] == SECONDARY else CONF_Y_AXIS
    return axis_multiplier(chart_config, x_axis), axis_multiplier(chart_config, y_axis)


async def chart_value(value: Any, multiplier: float) -> Any:
    """A chart value scaled to LVGL's integer points; NaN leaves a gap."""
    if isinstance(value, Lambda):
        return chart_point_value(await lv_float.process(value), multiplier)
    if (scaled := scale_value(value, multiplier)) is None:
        return literal("LV_CHART_POINT_NONE")
    return scaled


async def set_chart_options(
    chart: MockObj,
    config: ConfigType,
    chart_config: ConfigType,
    templated: bool | None = None,
) -> None:
    """Set the chart options in `config`, and the axis ranges that `templated` selects
    (all of them when it is None). `chart_config` is the chart's own configuration,
    which holds the axis multipliers."""
    if chart_type := config.get(CONF_TYPE):
        lv.chart_set_type(chart, literal(chart_type))
    if mode := config.get(CONF_UPDATE_MODE):
        lv.chart_set_update_mode(chart, literal(mode))
    if point_count := config.get(CONF_POINT_COUNT):
        lv.chart_set_point_count(chart, point_count)
    # Vertical grid lines are spaced along the X axis, horizontal ones along the Y axis
    if (lines := config.get(CONF_X_AXIS, {}).get(CONF_GRID_LINES)) is not None:
        lv.chart_set_ver_div_line_count(chart, lines)
    if (lines := config.get(CONF_Y_AXIS, {}).get(CONF_GRID_LINES)) is not None:
        lv.chart_set_hor_div_line_count(chart, lines)
    await set_axis_ranges(chart, config, chart_config, templated)


async def set_axis_ranges(
    chart: MockObj,
    config: ConfigType,
    chart_config: ConfigType,
    templated: bool | None = None,
) -> None:
    for axis_name, axis_key in AXES:
        axis = config.get(axis_name)
        if (
            axis is not None
            and CONF_MIN_VALUE in axis
            and templated in (None, _is_templated(axis))
        ):
            await set_axis_range(
                chart, axis_key, axis, axis_multiplier(chart_config, axis_name)
            )


async def set_axis_range(
    chart: MockObj, axis_key: str, axis: ConfigType, multiplier: float
) -> None:
    axis_id = literal(f"LV_CHART_AXIS_{axis_key}")
    min_value = await chart_value(axis[CONF_MIN_VALUE], multiplier)
    max_value = await chart_value(axis[CONF_MAX_VALUE], multiplier)
    if not _is_templated(axis):
        lv.chart_set_axis_range(chart, axis_id, min_value, max_value)
        return
    # The schema only rejects equal fixed values; a zero-width range would make
    # LVGL divide by zero when it draws a bar chart.
    with (
        LocalVariable("axis_min", cg.int32, min_value, modifier="") as low,
        LocalVariable("axis_max", cg.int32, max_value, modifier="") as high,
    ):
        lv.chart_set_axis_range(
            chart, axis_id, low, literal(f"{high} == {low} ? {low} + 1 : {high}")
        )


async def series_color(series: ConfigType, index: int) -> Any:
    if (color := series.get(CONF_COLOR)) is not None:
        return await lv_color.process(color)
    palette = SERIES_PALETTE[index % len(SERIES_PALETTE)]
    return lv_expr.palette_main(literal(f"LV_PALETTE_{palette}"))


def series_axes(series: ConfigType) -> MockObj:
    """The lv_chart_axis_t flags for the axes a series is drawn against."""
    y_axis = series[CONF_Y_AXIS] == SECONDARY
    x_axis = series[CONF_X_AXIS] == SECONDARY
    if y_axis and x_axis:
        return literal(
            "static_cast<lv_chart_axis_t>(LV_CHART_AXIS_SECONDARY_Y | LV_CHART_AXIS_SECONDARY_X)"
        )
    if x_axis:
        return literal("LV_CHART_AXIS_SECONDARY_X")
    return literal("LV_CHART_AXIS_SECONDARY_Y" if y_axis else "LV_CHART_AXIS_PRIMARY_Y")


async def set_series_values(
    chart: MockObj, series: MockObj, values: list, y_scale: float
) -> None:
    # Append in order so the series start point follows the data; writing slots by
    # index would leave the next appended value overwriting the first one.
    for value in values:
        lv.chart_set_next_value(chart, series, await chart_value(value, y_scale))


async def set_series_points(
    chart: MockObj, series: MockObj, points: list, x_scale: float, y_scale: float
) -> None:
    for point in points:
        lv.chart_set_next_value2(
            chart,
            series,
            await chart_value(point[CONF_X], x_scale),
            await chart_value(point[CONF_Y], y_scale),
        )


chart_spec = ChartType()


async def series_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: list,
    action: Callable,
) -> MockObj:
    """Generate a series action. `action` is called with the chart, the series, and
    the X and Y multipliers of the series' axes."""
    # Named from global scope, so a trigger argument such as `x` cannot shadow them
    series = await lv_global(config[CONF_ID])
    chart, series_config = get_chart_series()[str(config[CONF_ID])]
    x_scale, y_scale = series_multipliers(chart.config, series_config)

    async def do_action(w: Widget) -> None:
        obj = automation.parent_ref(w.obj)
        await action(obj, series, x_scale, y_scale)
        lv.chart_refresh(obj)

    return await action_to_code([chart], do_action, action_id, template_arg, args)


@automation.register_action(
    ACTION_APPEND, automation.ApplyAction, SERIES_APPEND_SCHEMA, synchronous=True
)
async def series_append_to_code(
    config: ConfigType, action_id: ID, template_arg: cg.TemplateArguments, args: list
) -> MockObj:
    async def append(
        chart: MockObj, series: MockObj, x_scale: float, y_scale: float
    ) -> None:
        if (point := config.get(CONF_POINT)) is not None:
            await set_series_points(chart, series, [point], x_scale, y_scale)
        else:
            await set_series_values(chart, series, [config[CONF_VALUE]], y_scale)

    return await series_action_to_code(config, action_id, template_arg, args, append)


@automation.register_action(
    ACTION_SET, automation.ApplyAction, SERIES_SET_SCHEMA, synchronous=True
)
async def series_set_to_code(
    config: ConfigType, action_id: ID, template_arg: cg.TemplateArguments, args: list
) -> MockObj:
    async def set_data(
        chart: MockObj, series: MockObj, x_scale: float, y_scale: float
    ) -> None:
        if CONF_INDEX not in config:
            # Clearing also moves the start point back to the first slot.
            lv.chart_set_all_values(chart, series, literal("LV_CHART_POINT_NONE"))
            if (points := config.get(CONF_POINTS)) is not None:
                await set_series_points(chart, series, points, x_scale, y_scale)
            else:
                await set_series_values(chart, series, config[CONF_VALUES], y_scale)
            return
        slot = chart_point_slot(
            chart, series, await lv_positive_int.process(config[CONF_INDEX])
        )
        if (point := config.get(CONF_POINT)) is not None:
            lv.chart_set_series_value_by_id2(
                chart,
                series,
                slot,
                await chart_value(point[CONF_X], x_scale),
                await chart_value(point[CONF_Y], y_scale),
            )
        else:
            lv.chart_set_series_value_by_id(
                chart, series, slot, await chart_value(config[CONF_VALUE], y_scale)
            )

    return await series_action_to_code(config, action_id, template_arg, args, set_data)


@automation.register_action(
    ACTION_CLEAR,
    automation.ApplyAction,
    cv.maybe_simple_value(SERIES_CLEAR_SCHEMA, key=CONF_ID),
    synchronous=True,
)
async def series_clear_to_code(
    config: ConfigType, action_id: ID, template_arg: cg.TemplateArguments, args: list
) -> MockObj:
    async def clear(
        chart: MockObj, series: MockObj, x_scale: float, y_scale: float
    ) -> None:
        lv.chart_set_all_values(chart, series, literal("LV_CHART_POINT_NONE"))

    return await series_action_to_code(config, action_id, template_arg, args, clear)


@automation.register_action(
    ACTION_UPDATE, automation.ApplyAction, SERIES_UPDATE_SCHEMA, synchronous=True
)
async def series_update_to_code(
    config: ConfigType, action_id: ID, template_arg: cg.TemplateArguments, args: list
) -> MockObj:
    async def update(
        chart: MockObj, series: MockObj, x_scale: float, y_scale: float
    ) -> None:
        if (color := config.get(CONF_COLOR)) is not None:
            lv.chart_set_series_color(chart, series, await lv_color.process(color))
        if (hidden := config.get(CONF_HIDDEN)) is not None:
            lv.chart_hide_series(chart, series, await lv_bool.process(hidden))

    return await series_action_to_code(config, action_id, template_arg, args, update)
