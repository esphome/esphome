import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "binary_sensor"]
MULTI_CONF = True

CONF_APDS9960_ID = "apds9960_id"
CONF_LED_DRIVE = "led_drive"
CONF_PROXIMITY_GAIN = "proximity_gain"
CONF_AMBIENT_LIGHT_GAIN = "ambient_light_gain"
CONF_GESTURE_LED_DRIVE = "gesture_led_drive"
CONF_GESTURE_GAIN = "gesture_gain"
CONF_GESTURE_WAIT_TIME = "gesture_wait_time"

DRIVE_LEVELS = {"100ma": 0, "50ma": 1, "25ma": 2, "12.5ma": 3}
PROXIMITY_LEVELS = {"1x": 0, "2x": 1, "4x": 2, "8x": 3}
AMBIENT_LEVELS = {"1x": 0, "4x": 1, "16x": 2, "64x": 3}
GESTURE_LEVELS = {"1x": 0, "2x": 1, "4x": 2, "8x": 3}
GESTURE_WAIT_TIMES = {
    "0ms": 0,
    "2.8ms": 1,
    "5.6ms": 2,
    "8.4ms": 3,
    "14ms": 4,
    "22.4ms": 5,
    "30.8ms": 6,
    "39.2ms": 7,
}

apds9960_nds = cg.esphome_ns.namespace("apds9960")
APDS9960 = apds9960_nds.class_("APDS9960", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(APDS9960),
            cv.Optional(CONF_LED_DRIVE, "100mA"): cv.enum(DRIVE_LEVELS, lower=True),
            cv.Optional(CONF_PROXIMITY_GAIN, "4x"): cv.enum(
                PROXIMITY_LEVELS, lower=True
            ),
            cv.Optional(CONF_AMBIENT_LIGHT_GAIN, "4x"): cv.enum(
                AMBIENT_LEVELS, lower=True
            ),
            cv.Optional(CONF_GESTURE_LED_DRIVE, "100mA"): cv.enum(
                DRIVE_LEVELS, lower=True
            ),
            cv.Optional(CONF_GESTURE_GAIN, "4x"): cv.enum(GESTURE_LEVELS, lower=True),
            cv.Optional(CONF_GESTURE_WAIT_TIME, "2.8ms"): cv.enum(
                GESTURE_WAIT_TIMES, lower=True
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
    cg.add(var.set_gesture_led_drive(config[CONF_GESTURE_LED_DRIVE]))
    cg.add(var.set_gesture_gain(config[CONF_GESTURE_GAIN]))
    cg.add(var.set_gesture_wait_time(config[CONF_GESTURE_WAIT_TIME]))
