import esphome.codegen as cg
from esphome.components import fan, output
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION_OUTPUT, CONF_OSCILLATION_OUTPUT, CONF_OUTPUT

from .. import binary_ns

BinaryFan = binary_ns.class_("BinaryFan", fan.Fan, cg.Component)

CONFIG_SCHEMA = (
    fan.fan_schema(BinaryFan)
    .extend(
        {
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
            cv.Optional(CONF_DIRECTION_OUTPUT): cv.use_id(output.BinaryOutput),
            cv.Optional(CONF_OSCILLATION_OUTPUT): cv.use_id(output.BinaryOutput),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)

    output_ = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(output_))

    if oscillation_output_id := config.get(CONF_OSCILLATION_OUTPUT):
        oscillation_output = await cg.get_variable(oscillation_output_id)
        cg.add(var.set_oscillating(oscillation_output))

    if direction_output_id := config.get(CONF_DIRECTION_OUTPUT):
        direction_output = await cg.get_variable(direction_output_id)
        cg.add(var.set_direction(direction_output))
