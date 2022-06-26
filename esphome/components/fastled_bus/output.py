import esphome.config_validation as cv
from esphome.components import output
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NUM_CHIPS, CONF_OFFSET
from . import CONF_BUS, CONF_REPEAT_DISTANCE, CONF_CHANNEL_OFFSET, bus_ns

clazz_output = bus_ns.class_("Output", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(clazz_output),
        cv.Required(CONF_BUS): cv.use_id(CONF_BUS),
        cv.Required(CONF_OFFSET): cv.positive_int,
        cv.Required(CONF_NUM_CHIPS): cv.positive_not_null_int,
        cv.Required(CONF_CHANNEL_OFFSET): cv.positive_int,
        cv.Optional(CONF_REPEAT_DISTANCE): cv.positive_not_null_int,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_NUM_CHIPS],
        config[CONF_OFFSET],
        config[CONF_CHANNEL_OFFSET],
    )
    bus = await cg.get_variable(config[CONF_BUS])
    if CONF_REPEAT_DISTANCE in config:
        cg.add(var.set_repeat_distance(config[CONF_REPEAT_DISTANCE]))
    cg.add(var.set_bus(bus))
    await cg.register_component(var, config)
    await output.register_output(var, config)
