import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_EXTERNAL_TEMPERATURE,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    CONF_ID,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

from .. import (
    uponor_smatrix_ns,
    UponorSmatrixDevice,
    UPONOR_SMATRIX_DEVICE_SCHEMA,
    register_uponor_smatrix_device,
)

DEPENDENCIES = ["uponor_smatrix"]

UponorSmatrixSensor = uponor_smatrix_ns.class_(
    "UponorSmatrixSensor",
    sensor.Sensor,
    cg.Component,
    UponorSmatrixDevice,
)

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(UponorSmatrixSensor),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_EXTERNAL_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(UPONOR_SMATRIX_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_uponor_smatrix_device(var, config)

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))
    if external_temperature_config := config.get(CONF_EXTERNAL_TEMPERATURE):
        sens = await sensor.new_sensor(external_temperature_config)
        cg.add(var.set_external_temperature_sensor(sens))
    if humidity_config := config.get(CONF_HUMIDITY):
        sens = await sensor.new_sensor(humidity_config)
        cg.add(var.set_humidity_sensor(sens))
