import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_EMPTY,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

DEPENDENCIES = ["i2c"]

aht10_ns = cg.esphome_ns.namespace("aht10")
AHT10Component = aht10_ns.class_("AHT10Component", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AHT10Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                UNIT_CELSIUS, ICON_EMPTY, 2, DEVICE_CLASS_TEMPERATURE
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                UNIT_PERCENT, ICON_EMPTY, 2, DEVICE_CLASS_HUMIDITY
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x38))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))
