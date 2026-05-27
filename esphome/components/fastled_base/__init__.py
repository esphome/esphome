import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_REFRESH_RATE,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_RGB_ORDER,
)
from esphome.core import CORE

CODEOWNERS = ["@OttoWinter"]
fastled_base_ns = cg.esphome_ns.namespace("fastled_base")
FastLEDLightOutput = fastled_base_ns.class_(
    "FastLEDLightOutput", light.AddressableLight
)

RGB_ORDERS = [
    "RGB",
    "RBG",
    "GRB",
    "GBR",
    "BRG",
    "BGR",
]

BASE_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FastLEDLightOutput),
        cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
        cv.Optional(CONF_RGB_ORDER): cv.one_of(*RGB_ORDERS, upper=True),
        cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def new_fastled_light(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)

    if CONF_MAX_REFRESH_RATE in config:
        cg.add(var.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))

    if CORE.is_esp32:
        # TEMPORARY: pin to swoboda1337's FastLED fork with the ESP-IDF 6
        # build fixes (see https://github.com/swoboda1337/FastLED/tree/
        # idf6-build-fixes). Revert to fastled/FastLED once an upstream
        # tagged release with these fixes is available.
        cg.add_library(
            "FastLED",
            None,
            "https://github.com/swoboda1337/FastLED.git#765840c6c64f1489d993a0f1ab3401ecc0b6356a",
        )
        # ESPHome disables the Arduino SPI library by default; FastLED needs
        # SPI.h, so opt in here.
        cg.add_library("SPI", None)

        from esphome.components.esp32 import include_builtin_idf_component

        include_builtin_idf_component("esp_lcd")
    else:
        cg.add_library("fastled/FastLED", "3.9.16")
    await light.register_light(var, config)
    return var
