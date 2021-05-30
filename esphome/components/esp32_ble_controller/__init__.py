import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_server, logger
from esphome.const import (
    CONF_BLE_SERVER_ID,
    CONF_ID,
    CONF_LEVEL,
    CONF_LOGGER,
    ESP_PLATFORM_ESP32,
)

AUTO_LOAD = ["esp32_ble_server"]
ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
CODEOWNERS = ["@jesserockz"]
CONFLICTS_WITH = ["esp32_ble_tracker", "esp32_ble_beacon"]

CONF_LOG_LEVEL = "log_level"

esp32_ble_controller_ns = cg.esphome_ns.namespace("esp32_ble_controller")
BLEController = esp32_ble_controller_ns.class_(
    "BLEController",
    cg.Component,
    cg.Controller,
    esp32_ble_server.BLEServiceComponent,
)


def validate(config, item_config):
    global_level = config[CONF_LOGGER][CONF_LEVEL]
    level = item_config.get(CONF_LOG_LEVEL, "DEBUG")
    if logger.LOG_LEVEL_SEVERITY.index(level) > logger.LOG_LEVEL_SEVERITY.index(
        global_level
    ):
        raise ValueError(
            "The esp32_ble_controller log level {} must be less severe than the "
            "global log level {}.".format(level, global_level)
        )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEController),
        cv.GenerateID(CONF_BLE_SERVER_ID): cv.use_id(esp32_ble_server.BLEServer),
        cv.Optional(CONF_LOG_LEVEL): cv.All(
            cv.requires_component("logger"),
            logger.is_log_level,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    ble_server = await cg.get_variable(config[CONF_BLE_SERVER_ID])
    cg.add(ble_server.register_service_component(var))

    if CONF_LOG_LEVEL in config:
        cg.add(var.set_log_level(logger.LOG_LEVELS[config[CONF_LOG_LEVEL]]))
