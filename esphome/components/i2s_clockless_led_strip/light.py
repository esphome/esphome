from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, light
from esphome.components.const import CONF_CHANNEL_COLORS
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_NUMBER, CONF_OUTPUT_ID, CONF_PIN
from esphome.types import ConfigType

CODEOWNERS = ["@j9brown"]
DEPENDENCIES = ["esp32"]

i2s_clockless_led_strip_ns = cg.esphome_ns.namespace("i2s_clockless_led_strip")
I2SClocklessLedStrip = i2s_clockless_led_strip_ns.class_(
    "I2SClocklessLedStrip", light.AddressableLight
)

# These variants define SOC_I2S_SUPPORTS_TDM
VARIANTS_WITH_I2S_TDM = {
    esp32.VARIANT_ESP32C3,
    esp32.VARIANT_ESP32C5,
    esp32.VARIANT_ESP32C6,
    esp32.VARIANT_ESP32C61,
    esp32.VARIANT_ESP32H2,
    esp32.VARIANT_ESP32H4,
    esp32.VARIANT_ESP32P4,
    esp32.VARIANT_ESP32S3,
}

CONFIG_SCHEMA = cv.All(
    esp32.only_on_variant(
        supported=list(VARIANTS_WITH_I2S_TDM),
        msg_prefix="I2S Clockless LED Strip",
    ),
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(I2SClocklessLedStrip),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Optional(CONF_CHANNEL_COLORS): light.validate_channel_colors,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    esp32.include_builtin_idf_component("esp_driver_i2s")
    esp32.add_idf_sdkconfig_option("CONFIG_I2S_ISR_IRAM_SAFE", True)

    var = cg.new_Pvariable(
        config[CONF_OUTPUT_ID],
        config[CONF_PIN][CONF_NUMBER],
        config[CONF_NUM_LEDS],
        light.channel_colors_struct(config[CONF_CHANNEL_COLORS]),
    )

    await light.register_light(var, config)
    await cg.register_component(var, config)
