from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    BUS_STATUS,
    CHARGE_STATUS,
    CONF_SY6970_ID,
    DATA_PUBLISHER,
    REGISTERS,
    SY6970Component,
    sy6970_ns,
)

DEPENDENCIES = ["sy6970"]

CONF_BUS_STATUS = "bus_status"
CONF_CHARGE_STATUS = "charge_status"
CONF_NTC_STATUS = "ntc_status"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SY6970_ID): cv.use_id(SY6970Component),
        cv.Optional(CONF_BUS_STATUS): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_CHARGE_STATUS): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_NTC_STATUS): text_sensor.text_sensor_schema(),
    }
)

STATES = {
    CONF_BUS_STATUS: sy6970_ns.bus_status_to_string(BUS_STATUS),
    CONF_CHARGE_STATUS: sy6970_ns.charge_status_to_string(CHARGE_STATUS),
    CONF_NTC_STATUS: sy6970_ns.ntc_status_to_string(
        REGISTERS[sy6970_ns.SY6970_REG_FAULT] & 0x07
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SY6970_ID])

    text_sensors = text_sensor.sub_text_sensors(config)
    for key, state in STATES.items():
        if var := await text_sensors(key, None):
            DATA_PUBLISHER.add(hub, parent_ref(var).publish_state(state))
