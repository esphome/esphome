import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_GAIN,
    CONF_ID,
    CONF_INTEGRATION_TIME,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)

DEPENDENCIES = ["i2c"]

tsl2561_ns = cg.esphome_ns.namespace("tsl2561")
TSL2561IntegrationTime = tsl2561_ns.enum("TSL2561IntegrationTime")
INTEGRATION_TIMES = {
    14: TSL2561IntegrationTime.TSL2561_INTEGRATION_14MS,
    101: TSL2561IntegrationTime.TSL2561_INTEGRATION_101MS,
    402: TSL2561IntegrationTime.TSL2561_INTEGRATION_402MS,
}

TSL2561Gain = tsl2561_ns.enum("TSL2561Gain")
GAINS = {
    "1X": TSL2561Gain.TSL2561_GAIN_1X,
    "16X": TSL2561Gain.TSL2561_GAIN_16X,
}

CONF_IS_CS_PACKAGE = "is_cs_package"


def validate_integration_time(value):
    value = cv.positive_time_period_milliseconds(value).total_milliseconds
    return cv.enum(INTEGRATION_TIMES, int=True)(value)


TSL2561Sensor = tsl2561_ns.class_(
    "TSL2561Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_LUX,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(TSL2561Sensor),
            cv.Optional(
                CONF_INTEGRATION_TIME, default="402ms"
            ): validate_integration_time,
            cv.Optional(CONF_GAIN, default="1X"): cv.enum(GAINS, upper=True),
            cv.Optional(CONF_IS_CS_PACKAGE, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x39))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_integration_time(config[CONF_INTEGRATION_TIME]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_is_cs_package(config[CONF_IS_CS_PACKAGE]))
