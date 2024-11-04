import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_BITS_PER_SAMPLE, CONF_ID, CONF_SAMPLE_RATE

CODEOWNERS = ["@kroimon", "@kahrendt"]
DEPENDENCIES = ["i2c"]

es8311_ns = cg.esphome_ns.namespace("es8311")
ES8311 = es8311_ns.class_("ES8311", AudioDac, cg.Component, i2c.I2CDevice)

CONF_MIC_GAIN = "mic_gain"
CONF_USE_MCLK = "use_mclk"
CONF_USE_MICROPHONE = "use_microphone"

es8311_resolution = es8311_ns.enum("ES8311Resolution")
ES8311_BITS_PER_SAMPLE_ENUM = {
    16: es8311_resolution.ES8311_RESOLUTION_16,
    24: es8311_resolution.ES8311_RESOLUTION_24,
    32: es8311_resolution.ES8311_RESOLUTION_32,
}

es8311_mic_gain = es8311_ns.enum("ES8311MicGain")
ES8311_MIC_GAIN_ENUM = {
    "MIN": es8311_mic_gain.ES8311_MIC_GAIN_MIN,
    "0DB": es8311_mic_gain.ES8311_MIC_GAIN_0DB,
    "6DB": es8311_mic_gain.ES8311_MIC_GAIN_6DB,
    "12DB": es8311_mic_gain.ES8311_MIC_GAIN_12DB,
    "18DB": es8311_mic_gain.ES8311_MIC_GAIN_18DB,
    "24DB": es8311_mic_gain.ES8311_MIC_GAIN_24DB,
    "30DB": es8311_mic_gain.ES8311_MIC_GAIN_30DB,
    "36DB": es8311_mic_gain.ES8311_MIC_GAIN_36DB,
    "42DB": es8311_mic_gain.ES8311_MIC_GAIN_42DB,
    "MAX": es8311_mic_gain.ES8311_MIC_GAIN_MAX,
}


_validate_bits = cv.float_with_unit("bits", "bit")

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES8311),
            cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): cv.All(
                _validate_bits, cv.enum(ES8311_BITS_PER_SAMPLE_ENUM)
            ),
            cv.Optional(CONF_MIC_GAIN, default="42DB"): cv.enum(
                ES8311_MIC_GAIN_ENUM, upper=True
            ),
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(min=1),
            cv.Optional(CONF_USE_MCLK, default=True): cv.boolean,
            cv.Optional(CONF_USE_MICROPHONE, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))
    cg.add(var.set_mic_gain(config[CONF_MIC_GAIN]))
    cg.add(var.set_sample_frequency(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_use_mclk(config[CONF_USE_MCLK]))
    cg.add(var.set_use_mic(config[CONF_USE_MICROPHONE]))
