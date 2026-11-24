import math
from typing import Any

from esphome import automation, codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR,
    CONF_ID,
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
from esphome.types import ConfigType

from ..automation import action_to_code
from ..defines import (
    CONF_CURSOR,
    CONF_INDICATOR,
    CONF_MAIN,
    CONF_POINTS,
    LV_CHART_TYPES,
    LValidator,
    LvConstant,
    get_chart_series,
    get_chart_series_updates,
    get_updated_widgets,
    literal,
)
from ..lv_validation import lv_color, lv_float
from ..lvcode import LocalVariable, lv, lv_assign, lv_expr, lv_global
from ..types import LvType, lvgl_ns
from . import Widget, WidgetType

CONF_CHART = "chart"
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


def _fixed(constant: LvConstant):
    """Accept one of the constant's values, but not a lambda."""

    def validator(value: Any) -> str:
        if isinstance(value, Lambda):
            raise cv.Invalid("This option cannot be a lambda")
        return constant.one_of(value)

    return validator


CHART_TYPE = _fixed(CHART_TYPES)
CHART_UPDATE_MODE = _fixed(CHART_UPDATE_MODES)

lv_chart_t = LvType("lv_chart_t")
lv_chart_series_t = cg.global_ns.struct("lv_chart_series_t")
chart_point_value = lvgl_ns.chart_point_value

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
    min_value = config[CONF_MIN_VALUE]
    max_value = config[CONF_MAX_VALUE]
    if isinstance(min_value, Lambda) or isinstance(max_value, Lambda):
        return
    if multiplier is None:
        multiplier = config[CONF_MULTIPLIER]
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
    if CONF_MIN_VALUE in config:
        _validate_range(config, 1.0)
    return config


def _axis_schema(grid_lines: bool) -> cv.All:
    """An axis of a new chart. Only the primary axes have grid lines, since the
    secondary axes share the same grid."""
    schema = {
        cv.Optional(CONF_MIN_VALUE, default=0): lv_finite_float,
        cv.Optional(CONF_MAX_VALUE, default=100): lv_finite_float,
        cv.Optional(CONF_MULTIPLIER, default=1.0): cv.All(
            cv.positive_not_null_float, _finite
        ),
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
        cv.Optional(CONF_Y_AXIS, default="primary"): SERIES_AXIS,
        cv.Optional(CONF_X_AXIS, default="primary"): SERIES_AXIS,
        cv.Optional(CONF_VALUES): cv.ensure_list(lv_float),
        cv.Optional(CONF_POINTS): cv.ensure_list(POINT_SCHEMA),
    }
)


def _record_series_update(config: ConfigType) -> ConfigType:
    """Remember the action so validate_chart_series_updates() can check it against
    its chart once the whole config is known."""
    get_chart_series_updates().append(config)
    return config


SERIES_MODIFY_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(lv_chart_series_t),
            cv.Optional(CONF_VALUE): lv_float,
            cv.Optional(CONF_VALUES): cv.ensure_list(lv_float),
            cv.Optional(CONF_POINT): POINT_SCHEMA,
        }
    ),
    cv.has_exactly_one_key(CONF_VALUE, CONF_VALUES, CONF_POINT),
    _record_series_update,
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


def validate_chart(config: ConfigType) -> ConfigType:
    point_count = config[CONF_POINT_COUNT]
    is_scatter = config[CONF_TYPE] == "LV_CHART_TYPE_SCATTER"
    for index, series in enumerate(config.get(CONF_SERIES, ())):
        values = series.get(CONF_VALUES)
        points = series.get(CONF_POINTS)
        if is_scatter and values is not None:
            raise cv.Invalid("A scatter chart series must use points instead of values")
        if not is_scatter and points is not None:
            raise cv.Invalid("points can only be used with a scatter chart")
        if not is_scatter and series[CONF_X_AXIS] == SECONDARY:
            raise cv.Invalid(
                "Only a scatter chart series can use the secondary X axis",
                path=[CONF_SERIES, index, CONF_X_AXIS],
            )
        data = points if is_scatter else values
        if data is not None and len(data) > point_count:
            raise cv.Invalid(
                f"A chart series can't have more than point_count ({point_count}) entries"
            )
    return config


def validate_series_update_for_type(
    config: ConfigType, chart_type: str, point_count: int
) -> ConfigType:
    is_scatter = chart_type == "LV_CHART_TYPE_SCATTER"
    if is_scatter and CONF_POINT not in config:
        raise cv.Invalid("A scatter chart series update requires point")
    if not is_scatter and CONF_POINT in config:
        raise cv.Invalid(
            "point can only update a scatter chart series", path=[CONF_POINT]
        )
    if (values := config.get(CONF_VALUES)) is not None and len(values) > point_count:
        raise cv.Invalid(
            f"A chart series update can't have more than point_count ({point_count}) values",
            path=[CONF_VALUES],
        )
    return config


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


def validate_chart_series_updates(global_config: FinalValidateConfig) -> None:
    """Check each lvgl.chart.series.update action against the chart its series belongs to."""
    smallest_point_counts = _smallest_point_counts(global_config)
    errors = []
    for config in get_chart_series_updates():
        series_id = config[CONF_ID]
        # A series ID is declared at <chart>/series/<index>/id
        chart_path = tuple(global_config.get_path_for_id(series_id)[:-3])
        chart_config = global_config.get_config_for_path(list(chart_path))
        point_count = min(
            chart_config[CONF_POINT_COUNT],
            smallest_point_counts.get(chart_path, chart_config[CONF_POINT_COUNT]),
        )
        try:
            validate_series_update_for_type(
                config, chart_config[CONF_TYPE], point_count
            )
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
                cv.Invalid(
                    f"'lvgl.chart.series.update' for series '{series_id}': {err.msg}",
                    path=path,
                )
            )
    if errors:
        raise cv.MultipleInvalid(errors)


class ChartType(WidgetType):
    def __init__(self) -> None:
        super().__init__(
            CONF_CHART,
            lv_chart_t,
            (CONF_MAIN, CONF_ITEMS, CONF_INDICATOR, CONF_CURSOR),
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


@automation.register_action(
    "lvgl.chart.series.update",
    automation.ApplyAction,
    cv.maybe_simple_value(SERIES_MODIFY_SCHEMA, key=CONF_ID),
    synchronous=True,
)
async def chart_series_update_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: list,
) -> MockObj:
    # Named from global scope, so a trigger argument such as `x` cannot shadow them
    series = await lv_global(config[CONF_ID])
    chart, series_config = get_chart_series()[str(config[CONF_ID])]
    x_scale, y_scale = series_multipliers(chart.config, series_config)

    async def do_update(w: Widget) -> None:
        obj = automation.parent_ref(w.obj)
        if (point := config.get(CONF_POINT)) is not None:
            await set_series_points(obj, series, [point], x_scale, y_scale)
        elif (values := config.get(CONF_VALUES)) is not None:
            # Clearing also moves the start point back to the first slot.
            lv.chart_set_all_values(obj, series, literal("LV_CHART_POINT_NONE"))
            await set_series_values(obj, series, values, y_scale)
        else:
            await set_series_values(obj, series, [config[CONF_VALUE]], y_scale)
        lv.chart_refresh(obj)

    return await action_to_code([chart], do_update, action_id, template_arg, args)
