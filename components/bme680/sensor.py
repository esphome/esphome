import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_PERCENT,
)

AUTO_LOAD = ["bme680"]

CONF_BME680_ID = "bme680_id"

bme680_ns = cg.esphome_ns.namespace("bme680")
BME680Component = bme680_ns.class_(
    "BME680Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_BME680_ID): cv.use_id(BME680Component),
            cv.Optional("temperature"): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("pressure"): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("humidity"): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x77))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if "temperature" in config:
        sens = await sensor.new_sensor(config["temperature"])
        cg.add(var.set_temperature_sensor(sens))

    if "pressure" in config:
        sens = await sensor.new_sensor(config["pressure"])
        cg.add(var.set_pressure_sensor(sens))

    if "humidity" in config:
        sens = await sensor.new_sensor(config["humidity"])
        cg.add(var.set_humidity_sensor(sens))
