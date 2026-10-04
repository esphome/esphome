import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_GAIN,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_INFRARED,
    CONF_VISIBLE,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_EMPTY,
    UNIT_LUX,
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@enwi"]

bh1730_ns = cg.esphome_ns.namespace("bh1730")
BH1730Component = bh1730_ns.class_(
    "BH1730Component", cg.PollingComponent, i2c.I2CDevice
)
BH1730Gain = bh1730_ns.enum("BH1730Gain")
GAIN_OPTIONS = {
    "1X": BH1730Gain.BH1730_GAIN_X1,
    "2X": BH1730Gain.BH1730_GAIN_X2,
    "64X": BH1730Gain.BH1730_GAIN_X64,
    "128X": BH1730Gain.BH1730_GAIN_X128,
}

CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Optional(CONF_VISIBLE): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_INFRARED): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_LUX,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_GAIN, default="1X"): cv.enum(GAIN_OPTIONS, upper=True),
    }
).extend(cv.polling_component_schema("60s"))

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    i2c.i2c_device_schema(default_address=0x29)
).extend({cv.GenerateID(): cv.declare_id(BH1730Component)})


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if visible_config := config.get(CONF_VISIBLE):
        sens = await sensor.new_sensor(visible_config)
        cg.add(var.set_visible_light_sensor(sens))

    if infrared_config := config.get(CONF_INFRARED):
        sens = await sensor.new_sensor(infrared_config)
        cg.add(var.set_infrared_light_sensor(sens))

    if illuminance_config := config.get(CONF_ILLUMINANCE):
        sens = await sensor.new_sensor(illuminance_config)
        cg.add(var.set_lux_light_sensor(sens))

    cg.add(var.set_gain(config[CONF_GAIN]))
