import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.const import CONF_CONNECTED
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from . import CONF_XEROX_PHASER_3020_ID, XeroxPhaser3020

DEPENDENCIES = ["xerox_phaser_3020"]

CONF_ERROR = "error"
CONF_ONLINE = "online"
CONF_PAPER_EMPTY = "paper_empty"
CONF_PRINTING = "printing"
CONF_TONER_LOW = "toner_low"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_XEROX_PHASER_3020_ID): cv.use_id(XeroxPhaser3020),
        cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_PRINTING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
            icon="mdi:printer",
        ),
        cv.Optional(CONF_PAPER_EMPTY): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            icon="mdi:tray-alert",
        ),
        # The printer's USB port status: an error such as a paper jam, an open cover or no toner
        cv.Optional(CONF_ERROR): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            icon="mdi:printer-alert",
        ),
        # From the printer's PJL status, read every status_interval
        cv.Optional(CONF_TONER_LOW): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            icon="mdi:printer-alert",
        ),
        # Whether the printer reports itself selected (online) on its USB port
        cv.Optional(CONF_ONLINE): binary_sensor.binary_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:printer-check",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_XEROX_PHASER_3020_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    await sensors(CONF_CONNECTED, hub.set_connected_binary_sensor)
    await sensors(CONF_PRINTING, hub.set_printing_binary_sensor)
    await sensors(CONF_PAPER_EMPTY, hub.set_paper_empty_binary_sensor)
    await sensors(CONF_ERROR, hub.set_error_binary_sensor)
    await sensors(CONF_ONLINE, hub.set_online_binary_sensor)
    await sensors(CONF_TONER_LOW, hub.set_toner_low_binary_sensor)
