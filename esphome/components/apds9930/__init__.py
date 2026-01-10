import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
MULTI_CONF = True
CODEOWNERS = ["@iyesin"]

CONF_APDS9930_ID = "apds9930_id"
CONF_LED_DRIVE = "led_drive"
CONF_PROXIMITY_GAIN = "proximity_gain"
CONF_AMBIENT_LIGHT_GAIN = "ambient_light_gain"
CONF_PROXIMITY_DIODE = "proximity_diode"

DRIVE_LEVELS = {"100ma": 0, "50ma": 1, "25ma": 2, "12.5ma": 3}
PROXIMITY_LEVELS = {"1x": 0, "2x": 1, "4x": 2, "8x": 3}
AMBIENT_LEVELS = {"1x": 0, "8x": 1, "16x": 2, "120x": 3}

apds9930_ns = cg.esphome_ns.namespace("apds9930")
APDS9930 = apds9930_ns.class_("APDS9930", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(APDS9930),
            cv.Optional(CONF_LED_DRIVE, default="100ma"): cv.enum(
                DRIVE_LEVELS, lower=True
            ),
            cv.Optional(CONF_PROXIMITY_GAIN, default="8x"): cv.enum(
                PROXIMITY_LEVELS, lower=True
            ),
            cv.Optional(CONF_AMBIENT_LIGHT_GAIN, default="1x"): cv.enum(
                AMBIENT_LEVELS, lower=True
            ),
            cv.Optional(CONF_PROXIMITY_DIODE, default=2): cv.int_range(
                min=0, max=3
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x39))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_led_drive(config[CONF_LED_DRIVE]))
    cg.add(var.set_proximity_gain(config[CONF_PROXIMITY_GAIN]))
    cg.add(var.set_ambient_gain(config[CONF_AMBIENT_LIGHT_GAIN]))
    cg.add(var.set_proximity_diode(config[CONF_PROXIMITY_DIODE]))
