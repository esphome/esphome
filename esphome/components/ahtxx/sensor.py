import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

DEPENDENCIES = ["i2c"]

ahtxx_ns = cg.esphome_ns.namespace("ahtxx")
AHTXXComponent = ahtxx_ns.class_("AHTXXComponent", cg.PollingComponent, i2c.I2CDevice)

CONF_TYPE = "type"
SensorType = ahtxx_ns.enum("SensorType")
SENSORTYPE = {
    "aht10": SensorType.AHT10,
    "aht2x": SensorType.AHT2X,
    "aht30": SensorType.AHT30,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AHTXXComponent),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TYPE, default="aht10") : cv.enum(SENSORTYPE),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x38))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_TYPE in config:
        cg.add(var.set_sensor_type(config[CONF_TYPE]))

    if temperature := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature)
        cg.add(var.set_temperature_sensor(sens))

    if humidity := config.get(CONF_HUMIDITY):
        sens = await sensor.new_sensor(humidity)
        cg.add(var.set_humidity_sensor(sens))
