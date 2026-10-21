import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_OUTPUT_ID, CONF_PIN
from esphome.types import ConfigType

from .. import CONF_SEESAW_ID, Seesaw, seesaw_ns

DEPENDENCIES = ["seesaw"]

SeesawNeopixel = seesaw_ns.class_("SeesawNeopixel", light.AddressableLight)

# The Seesaw firmware's NeoPixel buffer holds 512 bytes, so at most 170 RGB LEDs
MAX_LEDS = 170

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SeesawNeopixel),
        cv.GenerateID(CONF_SEESAW_ID): cv.use_id(Seesaw),
        cv.Required(CONF_PIN): cv.int_range(0, 31),
        cv.Optional(CONF_NUM_LEDS, default=1): cv.int_range(min=1, max=MAX_LEDS),
    }
)


async def to_code(config: ConfigType) -> None:
    num_leds = config[CONF_NUM_LEDS]
    # Four bytes per LED: GRB, then the effect data
    buf = f"{config[CONF_OUTPUT_ID].id}_buf"
    cg.add_global(cg.RawStatement(f"static uint8_t {buf}[{4 * num_leds}];"))
    var = await light.new_light(
        config, config[CONF_PIN], num_leds, cg.RawExpression(buf)
    )
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SEESAW_ID])
