import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, DEVICE_CLASS_POWER
from esphome.types import ConfigType

from .. import CONF_SY6970_ID, SY6970Component, sy6970_ns

DEPENDENCIES = ["sy6970"]

CONF_VBUS_CONNECTED = "vbus_connected"
CONF_CHARGING = "charging"
CONF_CHARGE_DONE = "charge_done"

SY6970VbusConnectedBinarySensor = sy6970_ns.class_(
    "SY6970VbusConnectedBinarySensor", binary_sensor.BinarySensor
)
SY6970ChargingBinarySensor = sy6970_ns.class_(
    "SY6970ChargingBinarySensor", binary_sensor.BinarySensor
)
SY6970ChargeDoneBinarySensor = sy6970_ns.class_(
    "SY6970ChargeDoneBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SY6970_ID): cv.use_id(SY6970Component),
        cv.Optional(CONF_VBUS_CONNECTED): binary_sensor.binary_sensor_schema(
            SY6970VbusConnectedBinarySensor,
            device_class=DEVICE_CLASS_CONNECTIVITY,
        ),
        cv.Optional(CONF_CHARGING): binary_sensor.binary_sensor_schema(
            SY6970ChargingBinarySensor,
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_CHARGE_DONE): binary_sensor.binary_sensor_schema(
            SY6970ChargeDoneBinarySensor,
            device_class=DEVICE_CLASS_POWER,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SY6970_ID])

    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_VBUS_CONNECTED, hub.add_listener)
    await binary_sensors(CONF_CHARGING, hub.add_listener)
    await binary_sensors(CONF_CHARGE_DONE, hub.add_listener)
