from esphome.components import display, sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.cpp_types import App
from esphome.const import (
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
    CONF_UPDATE_INTERVAL,
    CONF_X_GRID,
    CONF_Y_GRID,
    CONF_BORDER,
    CONF_AXES,
)

CODEOWNERS = ["@synco"]

DEPENDENCIES = ["display"]
MULTI_CONF = True

LineType = display.display_ns.enum("LineType")
LINE_TYPE = {
    "SOLID": LineType.LINE_TYPE_SOLID,
    "DOTTED": LineType.LINE_TYPE_DOTTED,
    "DASHED": LineType.LINE_TYPE_DASHED,
}

Graph_ = display.display_ns.class_("Graph")
graph_ns = cg.esphome_ns.namespace("graph")
GraphAxes = graph_ns.class_("GraphAxes")
GraphAxesPtr = GraphAxes.operator("ptr")

GRAPH_BASIC_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Graph_),
        cv.Required(CONF_WIDTH): cv.positive_int,
        cv.Required(CONF_HEIGHT): cv.positive_int,
    }
).extend(cv.polling_component_schema("10s"))

CONFIG_SCHEMA = GRAPH_BASIC_SCHEMA.extend(
    {
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_MIN_VALUE, default=float("nan")): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=float("nan")): cv.float_,
        cv.Optional(CONF_MIN_RANGE, default=float("nan")): cv.float_,
        cv.Optional(CONF_MAX_RANGE, default=float("nan")): cv.float_,
        cv.Optional(CONF_LINE_THICKNESS, default=3): cv.positive_int,
        cv.Optional(CONF_LINE_TYPE, default="SOLID"): cv.enum(LINE_TYPE, upper=True),
        cv.Optional(CONF_X_GRID, default=float("nan")): cv.float_,
        cv.Optional(CONF_Y_GRID, default=float("nan")): cv.float_,
        cv.Optional(CONF_BORDER, default=True): cv.boolean,
        cv.Optional(CONF_AXES): cv.All(
            cv.ensure_list(
                {
                    cv.GenerateID(): cv.declare_id(GraphAxes),
                    cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
                    cv.Optional(CONF_MIN_VALUE, default=float("nan")): cv.float_,
                    cv.Optional(CONF_MAX_VALUE, default=float("nan")): cv.float_,
                    cv.Optional(CONF_MIN_RANGE, default=float("nan")): cv.float_,
                    cv.Optional(CONF_MAX_RANGE, default=float("nan")): cv.float_,
                    cv.Optional(CONF_LINE_THICKNESS, default=3): cv.positive_int,
                    cv.Optional(CONF_LINE_TYPE, default="SOLID"): cv.enum(
                        LINE_TYPE, upper=True
                    ),
                }
            ),
            cv.Length(min=1),
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_WIDTH], config[CONF_HEIGHT])

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))
    cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    cg.add(var.set_min_range(config[CONF_MIN_RANGE]))
    cg.add(var.set_max_range(config[CONF_MAX_RANGE]))
    cg.add(var.set_line_thickness(config[CONF_LINE_THICKNESS]))
    cg.add(var.set_line_type(config[CONF_LINE_TYPE]))
    cg.add(var.set_grid_x(config[CONF_X_GRID]))
    cg.add(var.set_grid_y(config[CONF_Y_GRID]))
    cg.add(var.set_border(config[CONF_BORDER]))

    # TODO: This correct??? -since cg.register_component caused errors
    # await cg.register_component(var, config)
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(App.register_component(var))

    # TODO: add support for multiple traces on graph
    # if CONF_AXES in config:
    #     axes = []
    #     for conf in config[CONF_AXES]:
    #         axis = cg.new_Pvariable(conf[CONF_ID],...)
    #         axes.append(axis)
    #     cg.add(var.set_axis(axes))
