import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_GAIN,
    CONF_INTEGRATION_TIME,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["i2c"]

CONF_AUTO_GAIN = "auto_gain"
CONF_AUTO_GAIN_THRESHOLD_HIGH = "auto_gain_threshold_high"
CONF_AUTO_GAIN_THRESHOLD_LOW = "auto_gain_threshold_low"
CONF_DIGITAL_GAIN = "digital_gain"

veml3235_ns = cg.esphome_ns.namespace("veml3235")

VEML3235Sensor = veml3235_ns.class_(
    "VEML3235Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)
VEML3235IntegrationTime = veml3235_ns.enum("VEML3235IntegrationTime")
VEML3235_INTEGRATION_TIMES = {
    "50ms": VEML3235IntegrationTime.VEML3235_INTEGRATION_TIME_50MS,
    "100ms": VEML3235IntegrationTime.VEML3235_INTEGRATION_TIME_100MS,
    "200ms": VEML3235IntegrationTime.VEML3235_INTEGRATION_TIME_200MS,
    "400ms": VEML3235IntegrationTime.VEML3235_INTEGRATION_TIME_400MS,
    "800ms": VEML3235IntegrationTime.VEML3235_INTEGRATION_TIME_800MS,
}
VEML3235ComponentDigitalGain = veml3235_ns.enum("VEML3235ComponentDigitalGain")
DIGITAL_GAINS = {
    "1X": VEML3235ComponentDigitalGain.VEML3235_DIGITAL_GAIN_1X,
    "2X": VEML3235ComponentDigitalGain.VEML3235_DIGITAL_GAIN_2X,
}
VEML3235ComponentGain = veml3235_ns.enum("VEML3235ComponentGain")
GAINS = {
    "1X": VEML3235ComponentGain.VEML3235_GAIN_1X,
    "2X": VEML3235ComponentGain.VEML3235_GAIN_2X,
    "4X": VEML3235ComponentGain.VEML3235_GAIN_4X,
    "AUTO": VEML3235ComponentGain.VEML3235_GAIN_AUTO,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        VEML3235Sensor,
        unit_of_measurement=UNIT_LUX,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_DIGITAL_GAIN, default="1X"): cv.enum(
                DIGITAL_GAINS, upper=True
            ),
            cv.Optional(CONF_AUTO_GAIN, default=True): cv.boolean,
            cv.Optional(CONF_AUTO_GAIN_THRESHOLD_HIGH, default="90%"): cv.percentage,
            cv.Optional(CONF_AUTO_GAIN_THRESHOLD_LOW, default="20%"): cv.percentage,
            cv.Optional(CONF_GAIN, default="1X"): cv.enum(GAINS, upper=True),
            cv.Optional(CONF_INTEGRATION_TIME, default="50ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.enum(VEML3235_INTEGRATION_TIMES, lower=True),
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x10))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_auto_gain(config[CONF_AUTO_GAIN]))
    cg.add(var.set_auto_gain_threshold_high(config[CONF_AUTO_GAIN_THRESHOLD_HIGH]))
    cg.add(var.set_auto_gain_threshold_low(config[CONF_AUTO_GAIN_THRESHOLD_LOW]))
    cg.add(var.set_digital_gain(DIGITAL_GAINS[config[CONF_DIGITAL_GAIN]]))
    cg.add(var.set_gain(GAINS[config[CONF_GAIN]]))
    cg.add(var.set_integration_time(config[CONF_INTEGRATION_TIME]))
