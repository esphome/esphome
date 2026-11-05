import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, DEVICE_CLASS_POWER
from esphome.types import ConfigType

from .. import (
    BUS_STATUS,
    CHARGE_STATUS,
    CONF_SY6970_ID,
    SY6970Component,
    publish_children,
    sy6970_ns,
)

DEPENDENCIES = ["sy6970"]

CONF_VBUS_CONNECTED = "vbus_connected"
CONF_CHARGING = "charging"
CONF_CHARGE_DONE = "charge_done"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SY6970_ID): cv.use_id(SY6970Component),
        cv.Optional(CONF_VBUS_CONNECTED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
        ),
        cv.Optional(CONF_CHARGING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_CHARGE_DONE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_POWER,
        ),
    }
)

STATES = {
    CONF_VBUS_CONNECTED: BUS_STATUS != sy6970_ns.BUS_STATUS_NO_INPUT,
    CONF_CHARGING: sy6970_ns.is_charging(CHARGE_STATUS),
    CONF_CHARGE_DONE: CHARGE_STATUS == sy6970_ns.CHARGE_STATUS_CHARGE_DONE,
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SY6970_ID])

    await publish_children(hub, binary_sensor.sub_binary_sensors(config), STATES)
