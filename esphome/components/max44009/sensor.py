import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)

CODEOWNERS = ["@berfenger"]
DEPENDENCIES = ["i2c"]

max44009_ns = cg.esphome_ns.namespace("max44009")
MAX44009Sensor = max44009_ns.class_(
    "MAX44009Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

MAX44009Mode = max44009_ns.enum("MAX44009Mode")
MODE_OPTIONS = {
    "auto": MAX44009Mode.MAX44009_MODE_AUTO,
    "low_power": MAX44009Mode.MAX44009_MODE_LOW_POWER,
    "continuous": MAX44009Mode.MAX44009_MODE_CONTINUOUS,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_LUX,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(MAX44009Sensor),
            cv.Optional(CONF_MODE, default="low_power"): cv.enum(
                MODE_OPTIONS, lower=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x4A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_mode(config[CONF_MODE]))
