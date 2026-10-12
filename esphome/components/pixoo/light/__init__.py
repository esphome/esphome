import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_GAMMA_CORRECT, CONF_OUTPUT_ID
from esphome.types import ConfigType

from ..display import CONF_PIXOO_ID, Pixoo, pixoo_ns

PixooLight = pixoo_ns.class_("PixooLight", light.LightOutput)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(PixooLight),
        cv.GenerateID(CONF_PIXOO_ID): cv.use_id(Pixoo),
        # The LED board applies its own gamma, so default to no gamma correction here.
        cv.Optional(CONF_GAMMA_CORRECT, default=0.0): cv.positive_float,
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_parented(var, config[CONF_PIXOO_ID])
