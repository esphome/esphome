import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light

from esphome.const import CONF_OUTPUT_ID

from . import M5Angle8Component, m5angle8_ns, CONF_M5STACK_8ANGLE_ID


M5Angle8LightsComponent = m5angle8_ns.class_(
    "M5Angle8LightOutput",
    light.AddressableLight,
)


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_M5STACK_8ANGLE_ID): cv.use_id(M5Angle8Component),
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(M5Angle8LightsComponent),
        }
    )
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_M5STACK_8ANGLE_ID])
    lights = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(lights, config)
    await cg.register_component(lights, config)
    cg.add(lights.set_parent(hub))
