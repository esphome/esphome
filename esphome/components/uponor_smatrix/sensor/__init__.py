import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_EXTERNAL_TEMPERATURE,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TARGET_TEMPERATURE,
    CONF_TEMPERATURE,
)
from esphome.types import ConfigType

from .. import (
    UPONOR_SMATRIX_DEVICE_SCHEMA,
    UponorSmatrixDevice,
    register_uponor_smatrix_device,
    uponor_smatrix_ns,
)

DEPENDENCIES = ["uponor_smatrix"]

UponorSmatrixSensor = uponor_smatrix_ns.class_(
    "UponorSmatrixSensor",
    sensor.Sensor,
    UponorSmatrixDevice,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UponorSmatrixSensor),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(kind=sensor.TEMPERATURE),
        cv.Optional(CONF_EXTERNAL_TEMPERATURE): sensor.sensor_schema(
            kind=sensor.TEMPERATURE
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            kind=sensor.HUMIDITY, accuracy_decimals=0
        ),
        cv.Optional(CONF_TARGET_TEMPERATURE): sensor.sensor_schema(
            kind=sensor.TEMPERATURE
        ),
    }
).extend(UPONOR_SMATRIX_DEVICE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await register_uponor_smatrix_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_EXTERNAL_TEMPERATURE, var.set_external_temperature_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
    await sensors(CONF_TARGET_TEMPERATURE, var.set_target_temperature_sensor)
