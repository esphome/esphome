import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE_ID

from .. import Switch, switch_ns

CODEOWNERS = ["@ssieb"]

SwitchBinarySensor = switch_ns.class_(
    "SwitchBinarySensor", binary_sensor.BinarySensor, cg.Component
)


CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(SwitchBinarySensor)
    .extend(
        {
            cv.Required(CONF_SOURCE_ID): cv.use_id(Switch),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    source = await cg.get_variable(config[CONF_SOURCE_ID])
    cg.add(var.set_source(source))
