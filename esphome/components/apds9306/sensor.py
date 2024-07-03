# Based on this datasheet:
# https://www.mouser.ca/datasheet/2/678/AVGO_S_A0002854364_1-2574547.pdf

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_GAIN,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_LIGHTBULB,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)

DEPENDENCIES = ["i2c"]

CONF_APDS9306_ID = "apds9306_id"
CONF_BIT_WIDTH = "bit_width"
CONF_MEASUREMENT_RATE = "measurement_rate"

apds9306_ns = cg.esphome_ns.namespace("apds9306")
APDS9306 = apds9306_ns.class_(
    "APDS9306", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

MeasurementBitWidth = apds9306_ns.enum("MeasurementBitWidth")
MeasurementRate = apds9306_ns.enum("MeasurementRate")
AmbientLightGain = apds9306_ns.enum("AmbientLightGain")

MEASUREMENT_BIT_WIDTHS = {
    20: MeasurementBitWidth.MEASUREMENT_BIT_WIDTH_20,
    19: MeasurementBitWidth.MEASUREMENT_BIT_WIDTH_19,
    18: MeasurementBitWidth.MEASUREMENT_BIT_WIDTH_18,
    17: MeasurementBitWidth.MEASUREMENT_BIT_WIDTH_17,
    16: MeasurementBitWidth.MEASUREMENT_BIT_WIDTH_16,
    13: MeasurementBitWidth.MEASUREMENT_BIT_WIDTH_13,
}

MEASUREMENT_RATES = {
    25: MeasurementRate.MEASUREMENT_RATE_25,
    50: MeasurementRate.MEASUREMENT_RATE_50,
    100: MeasurementRate.MEASUREMENT_RATE_100,
    200: MeasurementRate.MEASUREMENT_RATE_200,
    500: MeasurementRate.MEASUREMENT_RATE_500,
    1000: MeasurementRate.MEASUREMENT_RATE_1000,
    2000: MeasurementRate.MEASUREMENT_RATE_2000,
}

AMBIENT_LIGHT_GAINS = {
    1: AmbientLightGain.AMBIENT_LIGHT_GAIN_1,
    3: AmbientLightGain.AMBIENT_LIGHT_GAIN_3,
    6: AmbientLightGain.AMBIENT_LIGHT_GAIN_6,
    9: AmbientLightGain.AMBIENT_LIGHT_GAIN_9,
    18: AmbientLightGain.AMBIENT_LIGHT_GAIN_18,
}


def _validate_measurement_rate(value):
    value = cv.positive_time_period_milliseconds(value)
    return cv.enum(MEASUREMENT_RATES, int=True)(value.total_milliseconds)


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        APDS9306,
        unit_of_measurement=UNIT_LUX,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
        icon=ICON_LIGHTBULB,
    )
    .extend(
        {
            cv.Optional(CONF_GAIN, default="1"): cv.enum(AMBIENT_LIGHT_GAINS, int=True),
            cv.Optional(CONF_BIT_WIDTH, default="18"): cv.enum(
                MEASUREMENT_BIT_WIDTHS, int=True
            ),
            cv.Optional(
                CONF_MEASUREMENT_RATE, default="100ms"
            ): _validate_measurement_rate,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x52))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_bit_width(config[CONF_BIT_WIDTH]))
    cg.add(var.set_measurement_rate(config[CONF_MEASUREMENT_RATE]))
    cg.add(var.set_ambient_light_gain(config[CONF_GAIN]))
