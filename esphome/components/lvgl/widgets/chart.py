from esphome import automation, codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_TYPE,
    CONF_VALUE,
    CONF_X,
    CONF_Y,
)

from ..automation import action_to_code
from ..defines import (
    CONF_CURSOR,
    CONF_INDICATOR,
    CONF_MAIN,
    CONF_POINTS,
    CONF_TICKS,
    LV_CHART_AXES,
    LV_CHART_TYPES,
    LvConstant,
    literal,
)
from ..lv_validation import lv_color, lv_int
from ..lvcode import lv, lv_assign, lv_expr
from ..types import LvType, ObjUpdateAction
from . import Widget, WidgetType, get_widgets
from .obj import obj_spec

CONF_CHART = "chart"
CONF_DIV_LINE_COUNT = "div_line_count"
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
CHART_AXES = LvConstant("LV_CHART_AXIS_", *LV_CHART_AXES)

lv_chart_t = LvType("lv_chart_t")
lv_chart_series_t = cg.global_ns.struct("lv_chart_series_t")

RANGE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MIN_VALUE, default=0): lv_int,
        cv.Optional(CONF_MAX_VALUE, default=100): lv_int,
    }
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
        cv.Optional(CONF_COLOR, default=0): lv_color,
        cv.Optional(CONF_Y_AXIS, default="PRIMARY_Y"): CHART_AXES.one_of,
        cv.Optional(CONF_VALUES): cv.ensure_list(lv_int),
        cv.Optional(CONF_POINTS): cv.ensure_list(POINT_SCHEMA),
    }
)

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
)

CHART_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TYPE, default="LINE"): CHART_TYPES.one_of,
        cv.Optional(CONF_UPDATE_MODE, default="SHIFT"): CHART_UPDATE_MODES.one_of,
        cv.Optional(CONF_POINT_COUNT, default=10): cv.int_range(min=1, max=4096),
        cv.Optional(CONF_DIV_LINE_COUNT): cv.ensure_list(cv.int_range(min=0, max=255)),
        cv.Optional(CONF_X_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_Y_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_SECONDARY_X_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_SECONDARY_Y_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_SERIES): cv.ensure_list(SERIES_SCHEMA),
    }
)

CHART_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_UPDATE_MODE): CHART_UPDATE_MODES.one_of,
        cv.Optional(CONF_POINT_COUNT): cv.int_range(min=1, max=4096),
        cv.Optional(CONF_DIV_LINE_COUNT): cv.ensure_list(cv.int_range(min=0, max=255)),
        cv.Optional(CONF_X_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_Y_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_SECONDARY_X_AXIS): RANGE_SCHEMA,
        cv.Optional(CONF_SECONDARY_Y_AXIS): RANGE_SCHEMA,
    }
)


def validate_div_line_count(config):
    if (div_count := config.get(CONF_DIV_LINE_COUNT)) and len(div_count) != 2:
        raise cv.Invalid(f"{CONF_DIV_LINE_COUNT} must contain exactly 2 values")
    return config


def validate_chart(config):
    validate_div_line_count(config)
    point_count = config[CONF_POINT_COUNT]
    is_scatter = config[CONF_TYPE] == "LV_CHART_TYPE_SCATTER"
    for series in config.get(CONF_SERIES, ()):
        values = series.get(CONF_VALUES)
        points = series.get(CONF_POINTS)
        if is_scatter and values is not None:
            raise cv.Invalid("A scatter chart series must use points instead of values")
        if not is_scatter and points is not None:
            raise cv.Invalid("points can only be used with a scatter chart")
        data = points if is_scatter else values
        if data is not None and len(data) > point_count:
            raise cv.Invalid(
                f"A chart series can't have more than point_count ({point_count}) entries"
            )
    return config


def validate_series_update_for_type(config, chart_type):
    is_scatter = chart_type == "LV_CHART_TYPE_SCATTER"
    if is_scatter and CONF_POINT not in config:
        raise cv.Invalid("A scatter chart series update requires point")
    if not is_scatter and CONF_POINT in config:
        raise cv.Invalid("point can only update a scatter chart series")
    return config


class ChartType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_CHART,
            lv_chart_t,
            (CONF_MAIN, CONF_INDICATOR, CONF_TICKS, CONF_CURSOR),
            CHART_SCHEMA.add_extra(validate_chart),
            CHART_MODIFY_SCHEMA.add_extra(validate_div_line_count),
        )

    async def to_code(self, w: Widget, config):
        if chart_type := config.get(CONF_TYPE):
            lv.chart_set_type(w.obj, literal(chart_type))
        if mode := config.get(CONF_UPDATE_MODE):
            lv.chart_set_update_mode(w.obj, literal(mode))
        if point_count := config.get(CONF_POINT_COUNT):
            lv.chart_set_point_count(w.obj, point_count)
        if div_count := config.get(CONF_DIV_LINE_COUNT):
            lv.chart_set_div_line_count(w.obj, div_count[0], div_count[1])

        for axis_name, axis_key in (
            (CONF_Y_AXIS, "PRIMARY_Y"),
            (CONF_SECONDARY_Y_AXIS, "SECONDARY_Y"),
            (CONF_X_AXIS, "PRIMARY_X"),
            (CONF_SECONDARY_X_AXIS, "SECONDARY_X"),
        ):
            if axis := config.get(axis_name):
                lv.chart_set_range(
                    w.obj,
                    literal(f"LV_CHART_AXIS_{axis_key}"),
                    await lv_int.process(axis[CONF_MIN_VALUE]),
                    await lv_int.process(axis[CONF_MAX_VALUE]),
                )

        for series in config.get(CONF_SERIES, ()):
            series_var = cg.Pvariable(
                series[CONF_ID], cg.nullptr, type_=lv_chart_series_t
            )
            lv_assign(
                series_var,
                lv_expr.chart_add_series(
                    w.obj,
                    await lv_color.process(series[CONF_COLOR]),
                    literal(series[CONF_Y_AXIS]),
                ),
            )
            series_config = dict(series)
            series_config[CONF_TYPE] = config[CONF_TYPE]
            series_widget = Widget.create(
                series[CONF_ID], w.obj, obj_spec, series_config
            )
            series_widget.obj = series_var
            if config[CONF_TYPE] == "LV_CHART_TYPE_SCATTER":
                await set_series_points(w.obj, series_var, series.get(CONF_POINTS, ()))
            else:
                await set_series_values(w.obj, series_var, series.get(CONF_VALUES, ()))

        lv.chart_refresh(w.obj)


async def set_series_values(chart, series, values):
    for index, value in enumerate(values):
        lv.chart_set_value_by_id(chart, series, index, await lv_int.process(value))


async def set_series_points(chart, series, points):
    for index, point in enumerate(points):
        lv.chart_set_series_value_by_id2(
            chart,
            series,
            index,
            await lv_int.process(point[CONF_X]),
            await lv_int.process(point[CONF_Y]),
        )


chart_spec = ChartType()


@automation.register_action(
    "lvgl.chart.series.update",
    ObjUpdateAction,
    cv.maybe_simple_value(SERIES_MODIFY_SCHEMA, key=CONF_ID),
    synchronous=True,
)
async def chart_series_update_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config)

    async def do_update(w: Widget):
        is_scatter = w.config[CONF_TYPE] == "LV_CHART_TYPE_SCATTER"
        validate_series_update_for_type(config, w.config[CONF_TYPE])
        if is_scatter:
            point = config[CONF_POINT]
            lv.chart_set_next_value2(
                w.var,
                w.obj,
                await lv_int.process(point[CONF_X]),
                await lv_int.process(point[CONF_Y]),
            )
        else:
            if values := config.get(CONF_VALUES):
                await set_series_values(w.var, w.obj, values)
            if (value := await lv_int.process(config.get(CONF_VALUE))) is not None:
                lv.chart_set_next_value(w.var, w.obj, value)
        lv.chart_refresh(w.var)

    return await action_to_code(
        widgets, do_update, action_id, template_arg, args, config
    )
