import esphome.codegen as cg
from esphome.components import fan, output
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION_OUTPUT, CONF_OSCILLATION_OUTPUT, CONF_OUTPUT
from esphome.types import ConfigType

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


async def to_code(config: ConfigType) -> None:
    var = await fan.new_fan(config)
    await cg.register_component(var, config)

    await cg.add_variable(var.set_output, config[CONF_OUTPUT])
    await cg.add_variable(var.set_oscillating, config.get(CONF_OSCILLATION_OUTPUT))
    await cg.add_variable(var.set_direction, config.get(CONF_DIRECTION_OUTPUT))
