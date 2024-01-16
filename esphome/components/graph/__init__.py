from esphome.components.font import Font
from esphome.components import sensor, color
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_COLOR,
    CONF_DIRECTION,
    CONF_DURATION,
    CONF_ID,
    CONF_LEGEND,
    CONF_NAME,
    CONF_NAME_FONT,
    CONF_SHOW_LINES,
    CONF_SHOW_UNITS,
    CONF_SHOW_VALUES,
    CONF_VALUE_FONT,
    CONF_WIDTH,
    CONF_SENSOR,
    CONF_HEIGHT,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_MIN_RANGE,
    CONF_MAX_RANGE,
    CONF_LINE_THICKNESS,
    CONF_LINE_TYPE,
    CONF_X_GRID,
    CONF_Y_GRID,
    CONF_BORDER,
    CONF_TRACES,
)

CODEOWNERS = ["@synco"]

DEPENDENCIES = ["display", "sensor"]
MULTI_CONF = True

graph_ns = cg.esphome_ns.namespace("graph")
Graph_ = graph_ns.class_("Graph", cg.Component)
GraphTrace = graph_ns.class_("GraphTrace")
GraphLegend = graph_ns.class_("GraphLegend")

LineType = graph_ns.enum("LineType")
LINE_TYPE = {
    "SOLID": LineType.LINE_TYPE_SOLID,
    "DOTTED": LineType.LINE_TYPE_DOTTED,
    "DASHED": LineType.LINE_TYPE_DASHED,
}

DirectionType = graph_ns.enum("DirectionType")
DIRECTION_TYPE = {
    "AUTO": DirectionType.DIRECTION_TYPE_AUTO,
    "HORIZONTAL": DirectionType.DIRECTION_TYPE_HORIZONTAL,
    "VERTICAL": DirectionType.DIRECTION_TYPE_VERTICAL,
}

ValuePositionType = graph_ns.enum("ValuePositionType")
VALUE_POSITION_TYPE = {
    "NONE": ValuePositionType.VALUE_POSITION_TYPE_NONE,
    "AUTO": ValuePositionType.VALUE_POSITION_TYPE_AUTO,
    "BESIDE": ValuePositionType.VALUE_POSITION_TYPE_BESIDE,
    "BELOW": ValuePositionType.VALUE_POSITION_TYPE_BELOW,
}

CONF_CONTINUOUS = "continuous"

GRAPH_TRACE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GraphTrace),
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_NAME): cv.string,
        cv.Optional(CONF_LINE_THICKNESS): cv.positive_int,
        cv.Optional(CONF_LINE_TYPE): cv.enum(LINE_TYPE, upper=True),
        cv.Optional(CONF_COLOR): cv.use_id(color.ColorStruct),
        cv.Optional(CONF_CONTINUOUS): cv.boolean,
    }
)

GRAPH_LEGEND_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(GraphLegend),
        cv.Required(CONF_NAME_FONT): cv.use_id(Font),
        cv.Optional(CONF_VALUE_FONT): cv.use_id(Font),
        cv.Optional(CONF_WIDTH): cv.positive_not_null_int,
        cv.Optional(CONF_HEIGHT): cv.positive_not_null_int,
        cv.Optional(CONF_BORDER): cv.boolean,
        cv.Optional(CONF_SHOW_LINES): cv.boolean,
        cv.Optional(CONF_SHOW_VALUES): cv.enum(VALUE_POSITION_TYPE, upper=True),
        cv.Optional(CONF_SHOW_UNITS): cv.boolean,
        cv.Optional(CONF_DIRECTION): cv.enum(DIRECTION_TYPE, upper=True),
    }
)


GRAPH_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Graph_),
        cv.Required(CONF_DURATION): cv.positive_time_period_seconds,
        cv.Required(CONF_WIDTH): cv.positive_not_null_int,
        cv.Required(CONF_HEIGHT): cv.positive_not_null_int,
        cv.Optional(CONF_X_GRID): cv.positive_time_period_seconds,
        cv.Optional(CONF_Y_GRID): cv.float_range(min=0, min_included=False),
        cv.Optional(CONF_BORDER): cv.boolean,
        # Single trace options in base
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_LINE_THICKNESS): cv.positive_int,
        cv.Optional(CONF_LINE_TYPE): cv.enum(LINE_TYPE, upper=True),
        cv.Optional(CONF_COLOR): cv.use_id(color.ColorStruct),
        # Axis specific options (Future feature may be to add second Y-axis)
        cv.Optional(CONF_MIN_VALUE): cv.float_,
        cv.Optional(CONF_MAX_VALUE): cv.float_,
        cv.Optional(CONF_MIN_RANGE): cv.float_range(min=0, min_included=False),
        cv.Optional(CONF_MAX_RANGE): cv.float_range(min=0, min_included=False),
        cv.Optional(CONF_TRACES): cv.ensure_list(GRAPH_TRACE_SCHEMA),
        cv.Optional(CONF_LEGEND): cv.ensure_list(GRAPH_LEGEND_SCHEMA),
    }
)


def _relocate_fields_to_subfolder(config, subfolder, subschema):
    fields = [k.schema for k in subschema.schema.keys()]
    fields.remove(CONF_ID)
    if subfolder in config:
        # Ensure no ambiguous fields in base of config
        for f in fields:
            if f in config:
                raise cv.Invalid(
                    "You cannot use the '"
                    + str(f)
                    + "' field when already using 'traces:'. "
                    "Please move it into 'traces:' entry."
                )
    else:
        # Copy over all fields to subfolder:
        trace = {}
        for f in fields:
            if f in config:
                trace[f] = config.pop(f)
        config[subfolder] = cv.ensure_list(subschema)(trace)
    return config


def _relocate_trace(config):
    return _relocate_fields_to_subfolder(config, CONF_TRACES, GRAPH_TRACE_SCHEMA)


CONFIG_SCHEMA = cv.All(
    GRAPH_SCHEMA,
    _relocate_trace,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_duration(config[CONF_DURATION]))
    cg.add(var.set_width(config[CONF_WIDTH]))
    cg.add(var.set_height(config[CONF_HEIGHT]))
    await cg.register_component(var, config)

    # Graph options
    if CONF_X_GRID in config:
        cg.add(var.set_grid_x(config[CONF_X_GRID]))
    if CONF_Y_GRID in config:
        cg.add(var.set_grid_y(config[CONF_Y_GRID]))
    if CONF_BORDER in config:
        cg.add(var.set_border(config[CONF_BORDER]))
    # Axis related options
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    if CONF_MIN_RANGE in config:
        cg.add(var.set_min_range(config[CONF_MIN_RANGE]))
    if CONF_MAX_RANGE in config:
        cg.add(var.set_max_range(config[CONF_MAX_RANGE]))
    # Trace options
    for trace in config[CONF_TRACES]:
        tr = cg.new_Pvariable(trace[CONF_ID], GraphTrace())
        sens = await cg.get_variable(trace[CONF_SENSOR])
        cg.add(tr.set_sensor(sens))
        if CONF_NAME in trace:
            cg.add(tr.set_name(trace[CONF_NAME]))
        else:
            cg.add(tr.set_name(trace[CONF_SENSOR].id))
        if CONF_LINE_THICKNESS in trace:
            cg.add(tr.set_line_thickness(trace[CONF_LINE_THICKNESS]))
        if CONF_LINE_TYPE in trace:
            cg.add(tr.set_line_type(trace[CONF_LINE_TYPE]))
        if CONF_COLOR in trace:
            c = await cg.get_variable(trace[CONF_COLOR])
            cg.add(tr.set_line_color(c))
        if CONF_CONTINUOUS in trace:
            cg.add(tr.set_continuous(trace[CONF_CONTINUOUS]))
        cg.add(var.add_trace(tr))
    # Add legend
    if CONF_LEGEND in config:
        lgd = config[CONF_LEGEND][0]
        legend = cg.new_Pvariable(lgd[CONF_ID], GraphLegend())
        if CONF_NAME_FONT in lgd:
            font = await cg.get_variable(lgd[CONF_NAME_FONT])
            cg.add(legend.set_name_font(font))
        if CONF_VALUE_FONT in lgd:
            font = await cg.get_variable(lgd[CONF_VALUE_FONT])
            cg.add(legend.set_value_font(font))
        if CONF_WIDTH in lgd:
            cg.add(legend.set_width(lgd[CONF_WIDTH]))
        if CONF_HEIGHT in lgd:
            cg.add(legend.set_height(lgd[CONF_HEIGHT]))
        if CONF_BORDER in lgd:
            cg.add(legend.set_border(lgd[CONF_BORDER]))
        if CONF_SHOW_LINES in lgd:
            cg.add(legend.set_lines(lgd[CONF_SHOW_LINES]))
        if CONF_SHOW_VALUES in lgd:
            cg.add(legend.set_values(lgd[CONF_SHOW_VALUES]))
        if CONF_SHOW_UNITS in lgd:
            cg.add(legend.set_units(lgd[CONF_SHOW_UNITS]))
        if CONF_DIRECTION in lgd:
            cg.add(legend.set_direction(lgd[CONF_DIRECTION]))
        cg.add(var.add_legend(legend))

    cg.add_define("USE_GRAPH")
