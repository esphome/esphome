import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_OUTPUT_ID, CONF_PIN

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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)
    seesaw = await cg.get_variable(config[CONF_SEESAW_ID])
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_parent(seesaw))
    cg.add(var.set_pin(config[CONF_PIN]))
