import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_LIGHT
from esphome.types import ConfigType

from ..climate import HaierTundra, haier_tundra_ns

CODEOWNERS = ["@fauxpark"]

LightButton = haier_tundra_ns.class_("LightButton", button.Button, cg.Component)
SelfCleanButton = haier_tundra_ns.class_("SelfCleanButton", button.Button, cg.Component)

ICON_LED_OUTLINE = "mdi:led-outline"
ICON_SPRAY_BOTTLE = "mdi:spray-bottle"

CONF_HAIER_TUNDRA_ID = "haier_tundra_id"
CONF_SELF_CLEAN = "self_clean"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_HAIER_TUNDRA_ID): cv.use_id(HaierTundra),
        cv.Optional(CONF_LIGHT): button.button_schema(
            LightButton,
            icon=ICON_LED_OUTLINE,
        ),
        cv.Optional(CONF_SELF_CLEAN): button.button_schema(
            SelfCleanButton,
            icon=ICON_SPRAY_BOTTLE,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_HAIER_TUNDRA_ID])

    if button_conf := config.get(CONF_LIGHT):
        var = cg.new_Pvariable(button_conf[cv.CONF_ID])
        await button.register_button(var, button_conf)
        await cg.register_component(var, button_conf)
        await cg.register_parented(var, parent)

    if button_conf := config.get(CONF_SELF_CLEAN):
        var = cg.new_Pvariable(button_conf[cv.CONF_ID])
        await button.register_button(var, button_conf)
        await cg.register_component(var, button_conf)
        await cg.register_parented(var, parent)
