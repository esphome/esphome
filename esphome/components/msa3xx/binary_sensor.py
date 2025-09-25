import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ACTIVE, CONF_NAME, DEVICE_CLASS_VIBRATION, ICON_VIBRATE

from . import CONF_MSA3XX_ID, MSA_SENSOR_SCHEMA

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["msa3xx"]

CONF_TAP = "tap"
CONF_DOUBLE_TAP = "double_tap"

ICON_TAP = "mdi:gesture-tap"
ICON_DOUBLE_TAP = "mdi:gesture-double-tap"

EVENT_SENSORS = (CONF_TAP, CONF_DOUBLE_TAP, CONF_ACTIVE)
ICONS = (ICON_TAP, ICON_DOUBLE_TAP, ICON_VIBRATE)

CONFIG_SCHEMA = MSA_SENSOR_SCHEMA.extend(
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
    hub = await cg.get_variable(config[CONF_MSA3XX_ID])

    for sensor in EVENT_SENSORS:
        if sensor in config:
            sens = await binary_sensor.new_binary_sensor(config[sensor])
            cg.add(getattr(hub, f"set_{sensor}_binary_sensor")(sens))
