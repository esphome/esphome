import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ACTIVE,
    CONF_NAME,
    DEVICE_CLASS_VIBRATION,
    ICON_VIBRATE,
)
from . import MSA3xxComponent, CONF_MSA3XX_ID

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["msa3xx"]

CONF_TAP = "tap"
CONF_DOUBLE_TAP = "double_tap"

ICON_TAP = "mdi:gesture-tap"
ICON_DOUBLE_TAP = "mdi:gesture-double-tap"


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_MSA3XX_ID): cv.use_id(MSA3xxComponent),
            cv.Optional(CONF_TAP): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_VIBRATION,
                    icon=ICON_TAP,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_DOUBLE_TAP): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_VIBRATION,
                    icon=ICON_DOUBLE_TAP,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_ACTIVE): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_VIBRATION,
                    icon=ICON_VIBRATE,
                ),
                key=CONF_NAME,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MSA3XX_ID])

    if CONF_TAP in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_TAP])
        cg.add(hub.set_tap_binary_sensor(sens))

    if CONF_DOUBLE_TAP in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DOUBLE_TAP])
        cg.add(hub.set_double_tap_binary_sensor(sens))

    if CONF_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_ACTIVE])
        cg.add(hub.set_active_binary_sensor(sens))
