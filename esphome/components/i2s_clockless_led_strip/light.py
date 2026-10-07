from dataclasses import dataclass

from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, light
from esphome.components.const import CONF_CHANNEL_COLORS
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_OUTPUT_ID, CONF_PIN
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@j9brown"]
DEPENDENCIES = ["esp32"]

CONF_I2S_CLOCKLESS_LED_STRIP = "i2s_clockless_led_strip"

i2s_clockless_led_strip_ns = cg.esphome_ns.namespace("i2s_clockless_led_strip")
I2SClocklessLedStrip = i2s_clockless_led_strip_ns.class_(
    "I2SClocklessLedStrip", light.AddressableLight
)

# These variants define SOC_I2S_SUPPORTS_TDM
I2S_TDM_PORTS = {
    esp32.VARIANT_ESP32C3: 1,
    esp32.VARIANT_ESP32C5: 1,
    esp32.VARIANT_ESP32C6: 1,
    esp32.VARIANT_ESP32C61: 1,
    esp32.VARIANT_ESP32H2: 1,
    esp32.VARIANT_ESP32H4: 1,
    esp32.VARIANT_ESP32P4: 3,
    esp32.VARIANT_ESP32S3: 2,
}

CONFIG_SCHEMA = cv.All(
    esp32.only_on_variant(
        supported=list(I2S_TDM_PORTS.keys()),
        msg_prefix="I2S Clockless LED Strip",
    ),
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(I2SClocklessLedStrip),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Required(CONF_CHANNEL_COLORS): light.validate_channel_colors,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


@dataclass
class I2SClocklessLedStripData:
    num_ports: int = 0


def _get_data() -> I2SClocklessLedStripData:
    if CONF_I2S_CLOCKLESS_LED_STRIP not in CORE.data:
        CORE.data[CONF_I2S_CLOCKLESS_LED_STRIP] = I2SClocklessLedStripData()
    return CORE.data[CONF_I2S_CLOCKLESS_LED_STRIP]


def _final_validate(_: ConfigType) -> None:
    _get_data().num_ports += 1
    variant = esp32.get_esp32_variant()
    num_ports_available = I2S_TDM_PORTS.get(variant, 0)
    if _get_data().num_ports > num_ports_available:
        raise cv.Invalid(
            f"Only {num_ports_available} I2S TDM ports are supported on {variant}"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    esp32.include_builtin_idf_component("esp_driver_i2s")
    esp32.add_idf_sdkconfig_option("CONFIG_I2S_ISR_IRAM_SAFE", True)

    var = cg.new_Pvariable(
        config[CONF_OUTPUT_ID],
        config[CONF_PIN],
        config[CONF_NUM_LEDS],
        light.channel_colors_struct(config[CONF_CHANNEL_COLORS]),
    )

    await light.register_light(var, config)
    await cg.register_component(var, config)
