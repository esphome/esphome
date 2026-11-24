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
from esphome.final_validate import FinalValidateConfig
from esphome.types import ConfigType

from ..automation import action_to_code
from ..defines import (
    CONF_CURSOR,
    CONF_INDICATOR,
    CONF_MAIN,
    CONF_POINTS,
    LV_CHART_TYPES,
    LvConstant,
    get_chart_series,
    get_chart_series_updates,
    get_updated_widgets,
    literal,
)
from ..lv_validation import lv_color, lv_int
from ..lvcode import LocalVariable, lv, lv_assign, lv_expr, lv_global
from ..types import LvType
from . import Widget, WidgetType

CONF_CHART = "chart"
CONF_DIV_LINE_COUNT = "div_line_count"
CONF_POINT_COUNT = "point_count"
CONF_POINT = "point"
CONF_SECONDARY_X_AXIS = "secondary_x_axis"
CONF_SECONDARY_Y_AXIS = "secondary_y_axis"
CONF_SERIES = "series"
CONF_UPDATE_MODE = "update_mode"
CONF_USE_SECONDARY_X_AXIS = "use_secondary_x_axis"
CONF_USE_SECONDARY_Y_AXIS = "use_secondary_y_axis"
CONF_VALUES = "values"
CONF_X_AXIS = "x_axis"
CONF_Y_AXIS = "y_axis"

CHART_TYPES = LvConstant("LV_CHART_TYPE_", *LV_CHART_TYPES)
CHART_UPDATE_MODES = LvConstant("LV_CHART_UPDATE_MODE_", "SHIFT", "CIRCULAR")
# Colours given in turn to series that do not set one
SERIES_PALETTE = ("BLUE", "RED", "GREEN", "ORANGE", "PURPLE", "TEAL")
DIV_LINE_COUNT_SCHEMA = cv.All(
    cv.ensure_list(cv.int_range(min=0, max=255)), cv.Length(min=2, max=2)
)


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


def _validate_range(config: ConfigType) -> ConfigType:
    """LVGL divides by the width of the range when it draws a bar chart."""
    min_value = config[CONF_MIN_VALUE]
    max_value = config[CONF_MAX_VALUE]
    if (
        not isinstance(min_value, Lambda)
        and not isinstance(max_value, Lambda)
        and min_value == max_value
    ):
        raise cv.Invalid(f"{CONF_MIN_VALUE} and {CONF_MAX_VALUE} must not be equal")
    return config


RANGE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MIN_VALUE, default=0): lv_int,
            cv.Optional(CONF_MAX_VALUE, default=100): lv_int,
        }
    ),
    _validate_range,
)

RANGE_MODIFY_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_MIN_VALUE): lv_int,
            cv.Required(CONF_MAX_VALUE): lv_int,
        }
    ),
    _validate_range,
)

POINT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_X): lv_int,
        cv.Required(CONF_Y): lv_int,
    }
)

SERIES_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(lv_chart_series_t),
        cv.Optional(CONF_COLOR): lv_color,
        cv.Optional(CONF_USE_SECONDARY_Y_AXIS, default=False): cv.boolean,
        cv.Optional(CONF_USE_SECONDARY_X_AXIS, default=False): cv.boolean,
        cv.Optional(CONF_VALUES): cv.ensure_list(lv_int),
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
            cv.Optional(CONF_VALUE): lv_int,
            cv.Optional(CONF_VALUES): cv.ensure_list(lv_int),
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
        cv.Optional(CONF_DIV_LINE_COUNT): DIV_LINE_COUNT_SCHEMA,
        cv.Optional(CONF_X_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_Y_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_SECONDARY_X_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_SECONDARY_Y_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_SERIES): cv.ensure_list(SERIES_SCHEMA),
    }
)

CHART_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_UPDATE_MODE): CHART_UPDATE_MODE,
        cv.Optional(CONF_POINT_COUNT): cv.int_range(min=1, max=4096),
        cv.Optional(CONF_DIV_LINE_COUNT): DIV_LINE_COUNT_SCHEMA,
        cv.Optional(CONF_X_AXIS): RANGE_MODIFY_SCHEMA,
        cv.Optional(CONF_Y_AXIS): RANGE_MODIFY_SCHEMA,
        cv.Optional(CONF_SECONDARY_X_AXIS): RANGE_MODIFY_SCHEMA,
        cv.Optional(CONF_SECONDARY_Y_AXIS): RANGE_MODIFY_SCHEMA,
    }
)


def validate_chart(config: ConfigType) -> ConfigType:
    point_count = config[CONF_POINT_COUNT]
    is_scatter = config[CONF_TYPE] == "LV_CHART_TYPE_SCATTER"
    for series in config.get(CONF_SERIES, ()):
        values = series.get(CONF_VALUES)
        points = series.get(CONF_POINTS)
        if is_scatter and values is not None:
            raise cv.Invalid("A scatter chart series must use points instead of values")
        if not is_scatter and points is not None:
            raise cv.Invalid("points can only be used with a scatter chart")
        if not is_scatter and series[CONF_USE_SECONDARY_X_AXIS]:
            raise cv.Invalid(
                f"{CONF_USE_SECONDARY_X_AXIS} can only be used with a scatter chart"
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

    async def create_to_code(self, config: ConfigType, parent: MockObj) -> Widget:
        # Options and series are set here rather than in to_code(), which
        # lvgl.widget.refresh runs again: it must not declare the series a second time
        # or undo what lvgl.chart.update changed. super() runs to_code(), which sets
        # the templated axis ranges.
        w = await super().create_to_code(config, parent)
        await set_chart_options(w.obj, config, templated=False)
        for index, series in enumerate(config.get(CONF_SERIES, ())):
            # Recorded before the variable exists, so an action waiting on it can find the chart.
            get_chart_series()[str(series[CONF_ID])] = w
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
            if config[CONF_TYPE] == "LV_CHART_TYPE_SCATTER":
                await set_series_points(w.obj, series_var, series.get(CONF_POINTS, ()))
            else:
                await set_series_values(w.obj, series_var, series.get(CONF_VALUES, ()))

        lv.chart_refresh(w.obj)
        return w

    async def update_to_code(self, w: Widget, config: ConfigType) -> None:
        await set_chart_options(w.obj, config)
        lv.chart_refresh(w.obj)

    async def to_code(self, w: Widget, config: ConfigType) -> None:
        await set_axis_ranges(w.obj, config, templated=True)
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


async def set_chart_options(
    chart: MockObj, config: ConfigType, templated: bool | None = None
) -> None:
    """Set the chart options in `config`, and the axis ranges that `templated` selects
    (all of them when it is None)."""
    if chart_type := config.get(CONF_TYPE):
        lv.chart_set_type(chart, literal(chart_type))
    if mode := config.get(CONF_UPDATE_MODE):
        lv.chart_set_update_mode(chart, literal(mode))
    if point_count := config.get(CONF_POINT_COUNT):
        lv.chart_set_point_count(chart, point_count)
    if div_count := config.get(CONF_DIV_LINE_COUNT):
        lv.chart_set_div_line_count(chart, div_count[0], div_count[1])
    await set_axis_ranges(chart, config, templated)


async def set_axis_ranges(
    chart: MockObj, config: ConfigType, templated: bool | None = None
) -> None:
    for axis_name, axis_key in AXES:
        if (axis := config.get(axis_name)) is not None and templated in (
            None,
            _is_templated(axis),
        ):
            await set_axis_range(chart, axis_key, axis)


async def set_axis_range(chart: MockObj, axis_key: str, axis: ConfigType) -> None:
    axis_id = literal(f"LV_CHART_AXIS_{axis_key}")
    min_value = await lv_int.process(axis[CONF_MIN_VALUE])
    max_value = await lv_int.process(axis[CONF_MAX_VALUE])
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
    y_axis = series[CONF_USE_SECONDARY_Y_AXIS]
    x_axis = series[CONF_USE_SECONDARY_X_AXIS]
    if y_axis and x_axis:
        return literal(
            "static_cast<lv_chart_axis_t>(LV_CHART_AXIS_SECONDARY_Y | LV_CHART_AXIS_SECONDARY_X)"
        )
    if x_axis:
        return literal("LV_CHART_AXIS_SECONDARY_X")
    return literal("LV_CHART_AXIS_SECONDARY_Y" if y_axis else "LV_CHART_AXIS_PRIMARY_Y")


async def set_series_values(chart: MockObj, series: MockObj, values: list) -> None:
    # Append in order so the series start point follows the data; writing slots by
    # index would leave the next appended value overwriting the first one.
    for value in values:
        lv.chart_set_next_value(chart, series, await lv_int.process(value))


async def set_series_points(chart: MockObj, series: MockObj, points: list) -> None:
    for point in points:
        lv.chart_set_next_value2(
            chart,
            series,
            await lv_int.process(point[CONF_X]),
            await lv_int.process(point[CONF_Y]),
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
    chart = get_chart_series()[str(config[CONF_ID])]

    async def do_update(w: Widget) -> None:
        obj = automation.parent_ref(w.obj)
        if (point := config.get(CONF_POINT)) is not None:
            lv.chart_set_next_value2(
                obj,
                series,
                await lv_int.process(point[CONF_X]),
                await lv_int.process(point[CONF_Y]),
            )
        elif (values := config.get(CONF_VALUES)) is not None:
            # Clearing also moves the start point back to the first slot.
            lv.chart_set_all_values(obj, series, literal("LV_CHART_POINT_NONE"))
            await set_series_values(obj, series, values)
        else:
            value = await lv_int.process(config[CONF_VALUE])
            lv.chart_set_next_value(obj, series, value)
        lv.chart_refresh(obj)

    return await action_to_code([chart], do_update, action_id, template_arg, args)
