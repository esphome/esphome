from esphome.components import sensor, color
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_COLOR,
    CONF_DURATION,
    CONF_ID,
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
Graph_ = graph_ns.class_("Graph")
GraphTrace = graph_ns.class_("GraphTrace")

LineType = graph_ns.enum("LineType")
LINE_TYPE = {
    "SOLID": LineType.LINE_TYPE_SOLID,
    "DOTTED": LineType.LINE_TYPE_DOTTED,
    "DASHED": LineType.LINE_TYPE_DASHED,
}

GRAPH_TRACE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GraphTrace),
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_LINE_THICKNESS): cv.positive_int,
        cv.Optional(CONF_LINE_TYPE): cv.enum(LINE_TYPE, upper=True),
        cv.Optional(CONF_COLOR): cv.use_id(color.ColorStruct),  # FIX
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
        cv.Optional(CONF_COLOR): cv.use_id(color.ColorStruct),  # FIX
        # Axis specific options (Future feature may be to add second Y-axis)
        cv.Optional(CONF_MIN_VALUE): cv.float_,
        cv.Optional(CONF_MAX_VALUE): cv.float_,
        cv.Optional(CONF_MIN_RANGE): cv.float_range(min=0, min_included=False),
        cv.Optional(CONF_MAX_RANGE): cv.float_range(min=0, min_included=False),
        cv.Optional(CONF_TRACES): cv.ensure_list(GRAPH_TRACE_SCHEMA),
    }
)


def _relocate_fields_to_subfolder(config, subfolder, subschema):
    fields = [k.schema for k in subschema.schema.keys()]
    fields.remove(CONF_ID)
    if subfolder in config:
        # Ensure no ambigious fields in base of config
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
    # await cg.register_component(var, config)           # Needed?

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
        if CONF_LINE_THICKNESS in trace:
            cg.add(tr.set_line_thickness(trace[CONF_LINE_THICKNESS]))
        if CONF_LINE_TYPE in trace:
            cg.add(tr.set_line_type(trace[CONF_LINE_TYPE]))
        if CONF_COLOR in trace:
            color = await cg.get_variable(trace[CONF_COLOR])
            cg.add(tr.set_line_color(color))
        cg.add(var.add_trace(tr))

    cg.add_define("USE_GRAPH")
