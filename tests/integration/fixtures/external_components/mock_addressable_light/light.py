import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_OUTPUT_ID
from esphome.types import ConfigType

mock_addressable_light_ns = cg.esphome_ns.namespace("mock_addressable_light")
MockAddressableLight = mock_addressable_light_ns.class_(
    "MockAddressableLight", light.AddressableLight
)

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(MockAddressableLight),
        cv.Optional(CONF_NUM_LEDS, default=4): cv.positive_not_null_int,
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_NUM_LEDS])
    await light.register_light(var, config)
    await cg.register_component(var, config)
