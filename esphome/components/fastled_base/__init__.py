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

    # Pinned to a GitHub SHA because the PlatformIO registry lags several releases behind upstream.
    # SHA b9e818437c3365584ce9f639386c2e2c78c81f44 (2026-03-20) is the last commit before the
    # 2026-03-21 math refactor that introduced SIMD headers which dont build on ESP8266.
    # It includes the IDF 6.0 compat fix (issue #2200) needed for IDF 6 builds.
    cg.add_library(
        "FastLED",
        None,
        "https://github.com/FastLED/FastLED.git#b9e818437c3365584ce9f639386c2e2c78c81f44",
    )
    if CORE.is_esp32:
        from esphome.components.esp32 import include_builtin_idf_component

        include_builtin_idf_component("esp_lcd")
    await light.register_light(var, config)
    return var
