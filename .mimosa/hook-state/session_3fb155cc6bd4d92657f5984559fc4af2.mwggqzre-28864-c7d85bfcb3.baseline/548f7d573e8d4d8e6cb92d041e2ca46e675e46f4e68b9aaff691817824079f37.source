import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_BATTERY
from esphome.types import ConfigType

from . import CONF_VEVOR_DTX_ID, VevorDtxComponent

DEPENDENCIES = ["vevor_dtx"]

CONF_LOW_BATTERY = "low_battery"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_VEVOR_DTX_ID): cv.use_id(VevorDtxComponent),
        cv.Optional(CONF_LOW_BATTERY): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BATTERY
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_VEVOR_DTX_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_LOW_BATTERY, hub.set_low_battery_binary_sensor)
