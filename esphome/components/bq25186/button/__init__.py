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

from .. import CONF_BQ25186_ID, BQ25186Component, bq25186_ns

DEPENDENCIES = ["bq25186"]

CONF_SOFTWARE_RESET = "software_reset"
CONF_SHUTDOWN = "shutdown"
CONF_SHIP_MODE = "ship_mode"
CONF_HARDWARE_RESET = "hardware_reset"

BQ25186SoftwareResetButton = bq25186_ns.class_(
    "BQ25186SoftwareResetButton", button.Button
)
BQ25186ShutdownButton = bq25186_ns.class_("BQ25186ShutdownButton", button.Button)
BQ25186ShipModeButton = bq25186_ns.class_("BQ25186ShipModeButton", button.Button)
BQ25186HardwareResetButton = bq25186_ns.class_(
    "BQ25186HardwareResetButton", button.Button
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25186_ID): cv.use_id(BQ25186Component),
        cv.Optional(CONF_SOFTWARE_RESET): button.button_schema(
            BQ25186SoftwareResetButton,
            device_class=DEVICE_CLASS_RESTART,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_RESTART_ALERT,
        ),
        cv.Optional(CONF_SHUTDOWN): button.button_schema(
            BQ25186ShutdownButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_POWER,
        ),
        cv.Optional(CONF_SHIP_MODE): button.button_schema(
            BQ25186ShipModeButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_POWER,
        ),
        cv.Optional(CONF_HARDWARE_RESET): button.button_schema(
            BQ25186HardwareResetButton,
            device_class=DEVICE_CLASS_RESTART,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_RESTART,
        ),
    }
)


async def to_code(config):
    if conf := config.get(CONF_SOFTWARE_RESET):
        b = await button.new_button(conf)
        await cg.register_parented(b, config[CONF_BQ25186_ID])

    if conf := config.get(CONF_SHUTDOWN):
        b = await button.new_button(conf)
        await cg.register_parented(b, config[CONF_BQ25186_ID])

    if conf := config.get(CONF_SHIP_MODE):
        b = await button.new_button(conf)
        await cg.register_parented(b, config[CONF_BQ25186_ID])

    if conf := config.get(CONF_HARDWARE_RESET):
        b = await button.new_button(conf)
        await cg.register_parented(b, config[CONF_BQ25186_ID])
