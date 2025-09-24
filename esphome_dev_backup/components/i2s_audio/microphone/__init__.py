from esphome import pins
import esphome.codegen as cg
from esphome.components import audio, esp32, microphone
from esphome.components.adc import ESP32_VARIANT_ADC1_PIN_TO_CHANNEL, validate_adc_pin
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_CHANNEL,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_NUMBER,
    CONF_SAMPLE_RATE,
)

from .. import (
    CONF_I2S_DIN_PIN,
    CONF_LEFT,
    CONF_MONO,
    CONF_RIGHT,
    I2SAudioIn,
    i2s_audio_component_schema,
    i2s_audio_ns,
    register_i2s_audio_component,
    use_legacy,
    validate_mclk_divisible_by_3,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2s_audio"]

CONF_ADC_PIN = "adc_pin"
CONF_ADC_TYPE = "adc_type"
CONF_CORRECT_DC_OFFSET = "correct_dc_offset"
CONF_PDM = "pdm"

I2SAudioMicrophone = i2s_audio_ns.class_(
    "I2SAudioMicrophone", I2SAudioIn, microphone.Microphone, cg.Component
)

INTERNAL_ADC_VARIANTS = [esp32.const.VARIANT_ESP32]
PDM_VARIANTS = [esp32.const.VARIANT_ESP32, esp32.const.VARIANT_ESP32S3]


def _validate_esp32_variant(config):
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


def _validate_channel(config):
    if config[CONF_CHANNEL] == CONF_MONO:
        raise cv.Invalid(f"I2S microphone does not support {CONF_MONO}.")
    return config


def _set_num_channels_from_config(config):
    if config[CONF_CHANNEL] in (CONF_LEFT, CONF_RIGHT):
        config[CONF_NUM_CHANNELS] = 1
    else:
        config[CONF_NUM_CHANNELS] = 2

    return config


def _set_stream_limits(config):
    audio.set_stream_limits(
        min_bits_per_sample=config.get(CONF_BITS_PER_SAMPLE),
        max_bits_per_sample=config.get(CONF_BITS_PER_SAMPLE),
        min_channels=config.get(CONF_NUM_CHANNELS),
        max_channels=config.get(CONF_NUM_CHANNELS),
        min_sample_rate=config.get(CONF_SAMPLE_RATE),
        max_sample_rate=config.get(CONF_SAMPLE_RATE),
    )(config)

    return config


BASE_SCHEMA = microphone.MICROPHONE_SCHEMA.extend(
    i2s_audio_component_schema(
        I2SAudioMicrophone,
        default_sample_rate=16000,
        default_channel=CONF_RIGHT,
        default_bits_per_sample="32bit",
    ).extend(
        {
            cv.Optional(CONF_CORRECT_DC_OFFSET, default=False): cv.boolean,
        }
    )
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
                    cv.Optional(CONF_PDM, default=False): cv.boolean,
                }
            ),
        },
        key=CONF_ADC_TYPE,
    ),
    _validate_esp32_variant,
    _validate_channel,
    _set_num_channels_from_config,
    _set_stream_limits,
    validate_mclk_divisible_by_3,
)


def _final_validate(config):
    if not use_legacy():
        if config[CONF_ADC_TYPE] == "internal":
            raise cv.Invalid("Internal ADC is only compatible with legacy i2s driver.")


FINAL_VALIDATE_SCHEMA = _final_validate


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

    cg.add(var.set_correct_dc_offset(config[CONF_CORRECT_DC_OFFSET]))
