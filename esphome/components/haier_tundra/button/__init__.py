import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_LIGHT
from esphome.types import ConfigType

from ..climate import HaierTundra, haier_tundra_ns

CODEOWNERS = ["@fauxpark"]

HaierTundraButton = haier_tundra_ns.class_(
    "HaierTundraButton", button.Button, cg.Component
)
ButtonKind = haier_tundra_ns.enum("ButtonKind", is_class=True)

ICON_LED_OUTLINE = "mdi:led-outline"
ICON_SPRAY_BOTTLE = "mdi:spray-bottle"

CONF_HAIER_TUNDRA_ID = "haier_tundra_id"
CONF_SELF_CLEAN = "self_clean"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_HAIER_TUNDRA_ID): cv.use_id(HaierTundra),
        cv.Optional(CONF_LIGHT): button.button_schema(
            HaierTundraButton, icon=ICON_LED_OUTLINE
        ),
        cv.Optional(CONF_SELF_CLEAN): button.button_schema(
            HaierTundraButton, icon=ICON_SPRAY_BOTTLE
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HAIER_TUNDRA_ID])
    for key, kind in (
        (CONF_LIGHT, ButtonKind.BUTTON_KIND_LIGHT),
        (CONF_SELF_CLEAN, ButtonKind.BUTTON_KIND_SELF_CLEAN),
    ):
        if (conf := config.get(key)) is not None:
            var = await button.new_button(conf, hub, kind)
            await cg.register_component(var, conf)
