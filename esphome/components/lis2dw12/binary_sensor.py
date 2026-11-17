import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.const import CONF_TAP
import esphome.config_validation as cv
from esphome.const import CONF_ACTIVE, CONF_NAME, DEVICE_CLASS_VIBRATION, ICON_VIBRATE
from esphome.types import ConfigType

from . import CONF_LIS2DW12_ID, LIS2DW12_SENSOR_SCHEMA

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["lis2dw12"]

CONF_DOUBLE_TAP = "double_tap"
CONF_FREEFALL = "freefall"

ICON_DOUBLE_TAP = "mdi:gesture-double-tap"
ICON_FREEFALL = "mdi:parachute"
ICON_TAP = "mdi:gesture-tap"

EVENT_SENSORS = (CONF_TAP, CONF_DOUBLE_TAP, CONF_FREEFALL, CONF_ACTIVE)
ICONS = (ICON_TAP, ICON_DOUBLE_TAP, ICON_FREEFALL, ICON_VIBRATE)

CONFIG_SCHEMA = LIS2DW12_SENSOR_SCHEMA.extend(
    {
        cv.Optional(event): cv.maybe_simple_value(
            binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_VIBRATION,
                icon=icon,
            ),
            key=CONF_NAME,
        )
        for event, icon in zip(EVENT_SENSORS, ICONS, strict=True)
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LIS2DW12_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    await sensors(CONF_TAP, hub.set_tap_binary_sensor)
    await sensors(CONF_DOUBLE_TAP, hub.set_double_tap_binary_sensor)
    await sensors(CONF_FREEFALL, hub.set_freefall_binary_sensor)
    await sensors(CONF_ACTIVE, hub.set_active_binary_sensor)
