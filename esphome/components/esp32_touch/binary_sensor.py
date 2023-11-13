import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_PIN,
    CONF_THRESHOLD,
    CONF_ID,
)
from . import esp32_touch_ns, ESP32TouchComponent, validate_touch_pad

DEPENDENCIES = ["esp32_touch", "esp32"]

CONF_ESP32_TOUCH_ID = "esp32_touch_id"
CONF_WAKEUP_THRESHOLD = "wakeup_threshold"

ESP32TouchBinarySensor = esp32_touch_ns.class_(
    "ESP32TouchBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(ESP32TouchBinarySensor).extend(
    {
        cv.GenerateID(CONF_ESP32_TOUCH_ID): cv.use_id(ESP32TouchComponent),
        cv.Required(CONF_PIN): validate_touch_pad,
        cv.Required(CONF_THRESHOLD): cv.uint32_t,
        cv.Optional(CONF_WAKEUP_THRESHOLD, default=0): cv.uint32_t,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ESP32_TOUCH_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_PIN],
        config[CONF_THRESHOLD],
        config[CONF_WAKEUP_THRESHOLD],
    )
    await binary_sensor.register_binary_sensor(var, config)
    cg.add(hub.register_touch_pad(var))
