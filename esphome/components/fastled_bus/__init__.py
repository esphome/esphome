import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import CONF_MAX_REFRESH_RATE, CONF_NUM_CHIPS, CONF_RGB_ORDER

CODEOWNERS = ["@mabels"]
AUTO_LOAD = ["output"]
CONF_BUS = "bus"

CONF_CHANNEL_OFFSET = "channel_offset"
CONF_REPEAT_DISTANCE = "repeat_distance"
CONF_CHIP_CHANNELS = "chip_channels"

RGB_ORDERS = [
    "RGB",
    "RBG",
    "GRB",
    "GBR",
    "BRG",
    "BGR",
]

bus_ns = cg.esphome_ns.namespace("fastled_bus")

FastLEDBus = bus_ns.class_("FastLEDBus", cg.Component)
CLEDControllerFactory = bus_ns.namespace("CLEDControllerFactory")

FastledBusId = cv.declare_id(FastLEDBus)

CONFIG_BUS_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.Required(CONF_NUM_CHIPS): cv.positive_not_null_int,
        # 3 means RGB for SK6812 you need 4
        cv.Optional(CONF_CHIP_CHANNELS, default=3): cv.positive_not_null_int,
        cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
        cv.Optional(CONF_RGB_ORDER): cv.one_of(*RGB_ORDERS, upper=True),
    }
)

REQUIRED_FRAMEWORK = cv.require_framework_version(
    esp8266_arduino=cv.Version(2, 7, 4),
    esp32_arduino=cv.Version(99, 0, 0),
    max_version=True,
    extra_message="Please see note on documentation for FastLED",
)


def rgb_order(config):
    return cg.RawExpression(
        config[CONF_RGB_ORDER] if CONF_RGB_ORDER in config else "RGB"
    )


def new_fastled_bus(var, config):
    if CONF_MAX_REFRESH_RATE in config:
        cg.add(var.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))

    # https://github.com/FastLED/FastLED/blob/master/library.json
    # 3.3.3 has an issue on ESP32 with RMT and fastled_clockless:
    # https://github.com/esphome/issues/issues/1375
    cg.add_library("fastled/FastLED", "3.3.2")
