# initially based off of TMP117 component

import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, DEVICE_CLASS_WIND_SPEED, STATE_CLASS_MEASUREMENT

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@kahrendt"]

fs3000_ns = cg.esphome_ns.namespace("fs3000")

FS3000Model = fs3000_ns.enum("MODEL")
FS3000_MODEL_OPTIONS = {
    "1005": FS3000Model.FIVE,
    "1015": FS3000Model.FIFTEEN,
}

FS3000Component = fs3000_ns.class_(
    "FS3000Component", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        FS3000Component,
        unit_of_measurement="m/s",
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_WIND_SPEED,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_MODEL): cv.enum(FS3000_MODEL_OPTIONS, lower=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))
