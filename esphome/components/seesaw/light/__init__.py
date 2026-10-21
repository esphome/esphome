import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_OUTPUT_ID, CONF_PIN
from esphome.types import ConfigType

from .. import CONF_SEESAW_ID, Seesaw, seesaw_ns

SeesawNeopixel = seesaw_ns.class_("SeesawNeopixel", light.AddressableLight)

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SeesawNeopixel),
        cv.GenerateID(CONF_SEESAW_ID): cv.use_id(Seesaw),
        cv.Required(CONF_PIN): cv.int_range(0, 31),
        cv.Optional(CONF_NUM_LEDS, default=1): cv.positive_not_null_int,
    }
)


async def to_code(config: ConfigType) -> None:
    var = await light.new_light(config, config[CONF_PIN])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SEESAW_ID])
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
