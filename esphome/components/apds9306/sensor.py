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

MEASUREMENT_BIT_WIDTHS = {
    20: 0,
    19: 1,
    18: 2,
    17: 3,
    16: 4,
    13: 5,
}

MEASUREMENT_RATES = {
    25: 0,
    50: 1,
    100: 2,
    200: 3,
    500: 4,
    1000: 5,
    2000: 6,
}

AMBIENT_LIGHT_GAINS = {
    1: 0,
    3: 1,
    6: 2,
    9: 3,
    18: 4,
}

apds9306_nds = cg.esphome_ns.namespace("apds9306")
APDS9306 = apds9306_nds.class_(
    "APDS9306", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

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
            cv.Optional(CONF_MEASUREMENT_RATE, default="100"): cv.All(
                cv.time_period_in_milliseconds_(),
                cv.enum(MEASUREMENT_RATES, int=True),
            ),
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
