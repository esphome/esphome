from esphome import pins
import esphome.codegen as cg
from esphome.components.one_wire import OneWireBus
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN, PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType

from .. import gpio_ns

CODEOWNERS = ["@ssieb"]

CONF_USE_RMT = "use_rmt"

GPIOOneWireBus = gpio_ns.class_("GPIOOneWireBus", OneWireBus, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GPIOOneWireBus),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_USE_RMT, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    use_rmt = config[CONF_USE_RMT]

    if use_rmt:
        if not CORE.is_esp32:
            raise cv.Invalid("use_rmt is only available on ESP32")

        from esphome.components import esp32_rmt
        from esphome.components.esp32 import (
            get_esp32_variant,
            include_builtin_idf_component,
        )

        variant = get_esp32_variant()
        if variant in esp32_rmt.VARIANTS_NO_RMT:
            raise cv.Invalid(f"RMT is not supported on ESP32 variant {variant}")

        # This define only controls whether RMT support is compiled into the
        # firmware. The transport selection itself is stored per bus instance.
        include_builtin_idf_component("esp_driver_rmt")
        cg.add_define("USE_ONE_WIRE_RMT")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_use_rmt(use_rmt))


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "gpio_one_wire_rmt.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "gpio_one_wire.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
            PlatformFramework.ESP8266_ARDUINO,
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
            PlatformFramework.RP2040_ARDUINO,
        },
    }
)
