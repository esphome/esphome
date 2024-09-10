from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, microphone
from esphome.components.adc import ESP32_VARIANT_ADC1_PIN_TO_CHANNEL, validate_adc_pin
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NUMBER

from .. import (
    CONF_I2S_DIN_PIN,
    CONF_RIGHT,
    INTERNAL_ADC_VARIANTS,
    PDM_VARIANTS,
    I2SAudioIn,
    i2s_audio_component_schema,
    i2s_audio_ns,
    register_i2s_audio_component,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2s_audio"]

CONF_ADC_PIN = "adc_pin"
CONF_ADC_TYPE = "adc_type"
CONF_PDM = "pdm"

CONF_USE_APLL = "use_apll"

I2SAudioMicrophone = i2s_audio_ns.class_(
    "I2SAudioMicrophone", I2SAudioIn, microphone.Microphone, cg.Component
)


def validate_esp32_variant(config):
    variant = esp32.get_esp32_variant()
    if config[CONF_ADC_TYPE] == "external":
        if config[CONF_PDM]:
            if variant not in PDM_VARIANTS:
                raise cv.Invalid(f"{variant} does not support PDM")
        return config
    if config[CONF_ADC_TYPE] == "internal":
        if variant not in INTERNAL_ADC_VARIANTS:
            raise cv.Invalid(f"{variant} does not have an internal ADC")
        return config
    raise NotImplementedError


BASE_SCHEMA = microphone.MICROPHONE_SCHEMA.extend(
    i2s_audio_component_schema(I2SAudioMicrophone, 16000, CONF_RIGHT, "32bit")
).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "internal": BASE_SCHEMA.extend(
                {
                    cv.Required(CONF_ADC_PIN): validate_adc_pin,
                }
            ),
            "external": BASE_SCHEMA.extend(
                {
                    cv.Required(CONF_I2S_DIN_PIN): pins.internal_gpio_input_pin_number,
                    cv.Required(CONF_PDM): cv.boolean,
                    cv.Optional(CONF_USE_APLL, default=False): cv.boolean,
                }
            ),
        },
        key=CONF_ADC_TYPE,
    ),
    validate_esp32_variant,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_i2s_audio_component(var, config)
    await microphone.register_microphone(var, config)

    if config[CONF_ADC_TYPE] == "internal":
        variant = esp32.get_esp32_variant()
        pin_num = config[CONF_ADC_PIN][CONF_NUMBER]
        channel = ESP32_VARIANT_ADC1_PIN_TO_CHANNEL[variant][pin_num]
        cg.add(var.set_adc_channel(channel))
    else:
        cg.add(var.set_din_pin(config[CONF_I2S_DIN_PIN]))
        cg.add(var.set_pdm(config[CONF_PDM]))
        cg.add(var.set_use_apll(config[CONF_USE_APLL]))
