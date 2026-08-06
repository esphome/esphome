import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ACTIVE, CONF_NAME, DEVICE_CLASS_VIBRATION, ICON_VIBRATE

from . import CONF_LIS2DH12_ID, LIS2DH12_SENSOR_SCHEMA

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["lis2dh12_base"]

CONF_TAP = "tap"
CONF_DOUBLE_TAP = "double_tap"
CONF_FREEFALL = "freefall"

ICON_TAP = "mdi:gesture-tap"
ICON_DOUBLE_TAP = "mdi:gesture-double-tap"
ICON_FREEFALL = "mdi:parachute"

EVENT_SENSORS = (CONF_TAP, CONF_DOUBLE_TAP, CONF_FREEFALL, CONF_ACTIVE)
ICONS = (ICON_TAP, ICON_DOUBLE_TAP, ICON_FREEFALL, ICON_VIBRATE)

CONFIG_SCHEMA = LIS2DH12_SENSOR_SCHEMA.extend(
    {
        cv.Optional(event): cv.maybe_simple_value(
            binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_VIBRATION,
                icon=icon,
            ),
            key=CONF_NAME,
        )
        for event, icon in zip(EVENT_SENSORS, ICONS)
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LIS2DH12_ID])

    for sensor_key in EVENT_SENSORS:
        if sensor_key in config:
            sens = await binary_sensor.new_binary_sensor(config[sensor_key])
            cg.add(getattr(hub, f"set_{sensor_key}_binary_sensor")(sens))
