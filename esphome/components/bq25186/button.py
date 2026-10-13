import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_POWER,
    ICON_RESTART,
    ICON_RESTART_ALERT,
)
from esphome.types import ConfigType

from . import BQ25186_CHILD_SCHEMA, CONF_BQ25186_ID, bq25186_ns

DEPENDENCIES = ["bq25186"]

BQ25186Button = bq25186_ns.class_("BQ25186Button", button.Button)

# SHIP_RST: bit 7 is the software reset, bits 6:5 the shutdown, ship and hardware reset codes
SOFTWARE_RESET_MASK = 0x80
MODE_MASK = 0x60

# (key, SHIP_RST mask, value, device class, icon)
_BUTTONS = (
    ("software_reset", SOFTWARE_RESET_MASK, 0x80, DEVICE_CLASS_RESTART, ICON_RESTART),
    ("shutdown", MODE_MASK, 0x20, cv.UNDEFINED, ICON_POWER),
    ("ship_mode", MODE_MASK, 0x40, cv.UNDEFINED, ICON_POWER),
    ("hardware_reset", MODE_MASK, 0x60, DEVICE_CLASS_RESTART, ICON_RESTART_ALERT),
)


CONFIG_SCHEMA = cv.All(
    BQ25186_CHILD_SCHEMA.extend(
        {
            cv.Optional(key): button.button_schema(
                BQ25186Button,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=icon,
                device_class=device_class,
            )
            for key, _, _, device_class, icon in _BUTTONS
        }
    ),
    cv.has_at_least_one_key(*(key for key, *_ in _BUTTONS)),
)


async def to_code(config: ConfigType) -> None:
    for key, mask, value, _, _ in _BUTTONS:
        if (conf := config.get(key)) is not None:
            var = await button.new_button(conf, mask, value)
            await cg.register_parented(var, config[CONF_BQ25186_ID])
