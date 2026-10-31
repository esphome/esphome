import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from . import CONF_MODEM_ID, ModemComponent, request_polling

DEPENDENCIES = ["modem"]

CONF_BER = "ber"
CONF_RSSI = "rssi"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MODEM_ID): cv.use_id(ModemComponent),
        cv.Optional(CONF_RSSI): sensor.sensor_schema(
            kind=sensor.SIGNAL_STRENGTH,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_BER): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=2,
            icon="mdi:signal",
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    if CONF_RSSI not in config and CONF_BER not in config:
        return
    hub = await cg.get_variable(config[CONF_MODEM_ID])
    request_polling(hub)
    cg.add_define("USE_MODEM_SIGNAL")
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_RSSI, hub.set_rssi_sensor)
    await sensors(CONF_BER, hub.set_ber_sensor)
