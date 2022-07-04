import esphome.codegen as cg
from esphome.components import fastled_bus
from esphome.components.fastled_bus import CONF_BUS
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_NUM_LEDS,
    CONF_RGB_ORDER,
    CONF_MAX_REFRESH_RATE,
)

CODEOWNERS = ["@OttoWinter"]
fastled_base_ns = cg.esphome_ns.namespace("fastled_base")
FastLEDLightOutput = fastled_base_ns.class_(
    "FastLEDLightOutput", light.AddressableLight
)


BASE_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FastLEDLightOutput),
        cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
        cv.Optional(CONF_RGB_ORDER): cv.one_of(*fastled_bus.RGB_ORDERS, upper=True),
        cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
        cv.Optional(CONF_BUS): cv.declare_id(fastled_bus.FastLEDBus),
    }
).extend(cv.COMPONENT_SCHEMA)


async def new_fastled_light(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)

    if CONF_MAX_REFRESH_RATE in config:
        cg.add(var.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))

    await light.register_light(var, config)
    # https://github.com/FastLED/FastLED/blob/master/library.json
    # 3.3.3 has an issue on ESP32 with RMT and fastled_clockless:
    # https://github.com/esphome/issues/issues/1375
    cg.add_library("fastled/FastLED", "3.3.2")
    return var
