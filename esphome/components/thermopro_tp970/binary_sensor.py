import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_THERMOPRO_TP970_ID, ThermoProTP970

CONF_TEMPERATURE_STREAM = "temperature_stream"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_THERMOPRO_TP970_ID): cv.use_id(ThermoProTP970),
        cv.Optional(CONF_TEMPERATURE_STREAM): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_THERMOPRO_TP970_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    await sensors(CONF_TEMPERATURE_STREAM, hub.set_temperature_stream_binary_sensor)
