import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    BUS_STATUS,
    CHARGE_STATUS,
    CONF_SY6970_ID,
    REGISTERS,
    SY6970Component,
    publish_children,
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


def _status_string(table: str, index: cg.MockObj) -> cg.MockObj:
    """The string at ``index`` in the hub's PROGMEM_STRING_TABLE ``table``."""
    strings = sy6970_ns.namespace(table)
    return strings.get_progmem_str(index, strings.LAST_INDEX)


STATES = {
    CONF_BUS_STATUS: _status_string("BusStatusStrings", BUS_STATUS),
    CONF_CHARGE_STATUS: _status_string("ChargeStatusStrings", CHARGE_STATUS),
    CONF_NTC_STATUS: _status_string(
        "NtcStatusStrings", REGISTERS[sy6970_ns.SY6970_REG_FAULT] & 0x07
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SY6970_ID])

    await publish_children(hub, text_sensor.sub_text_sensors(config), STATES)
