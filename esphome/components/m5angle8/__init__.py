import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.automation import maybe_simple_id


AUTO_LOAD = ["sensor", "light"]
DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@rnauber",]
MULTI_CONF = True

NUMBER_KNOBS = 8
NUMBER_LEDS = 9

from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    ICON_ROTATE_RIGHT,
    CONF_CHANNEL,
)


CONF_KNOB_POSITION_PREFIX = "knob_position_"

m5angle8_ns = cg.esphome_ns.namespace("m5angle8")
M5Angle8Component = m5angle8_ns.class_(
    "M5Angle8Component", cg.PollingComponent, i2c.I2CDevice
)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5Angle8Component),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x43))
    .extend({cv.Optional(CONF_KNOB_POSITION_PREFIX + str(i)): sensor.sensor_schema(
                accuracy_decimals=2,
                icon=ICON_ROTATE_RIGHT,
                state_class=STATE_CLASS_MEASUREMENT,
        ) for i in range(NUMBER_KNOBS)})
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
   
    for i in range(NUMBER_KNOBS):
        conf_key = CONF_KNOB_POSITION_PREFIX + str(i)
        if conf_key in config:
            sens = await sensor.new_sensor(config[conf_key])
            cg.add(var.set_sens_knob_position(i, sens))
        
        
