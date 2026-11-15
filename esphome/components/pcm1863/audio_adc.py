import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_adc import AudioAdc
import esphome.config_validation as cv
from esphome.const import CONF_BITS_PER_SAMPLE, CONF_INPUT, CONF_MIC_GAIN
from esphome.types import ConfigType

CODEOWNERS = ["@anabolyc"]
DEPENDENCIES = ["i2c"]

CONF_PCM1863_ID = "pcm1863_id"

# The PGA covers -12 dB to +40 dB in 0.5 dB steps
PCM1863_MIC_GAIN_MIN = -12.0
PCM1863_MIC_GAIN_MAX = 40.0

pcm1863_ns = cg.esphome_ns.namespace("pcm1863")
PCM1863 = pcm1863_ns.class_("PCM1863", AudioAdc, cg.Component, i2c.I2CDevice)

pcm1863_input = pcm1863_ns.enum("PCM1863Input")
# The declaration order is the option order of the select platform, and has to stay in
# step with INPUT_BY_INDEX in pcm1863.cpp
PCM1863_INPUT_ENUM = {
    "NONE": pcm1863_input.PCM1863_INPUT_SEL_NONE,
    "VIN1": pcm1863_input.PCM1863_INPUT_SEL_VIN1,
    "VIN2": pcm1863_input.PCM1863_INPUT_SEL_VIN2,
    "VIN3": pcm1863_input.PCM1863_INPUT_SEL_VIN3,
    "VIN4": pcm1863_input.PCM1863_INPUT_SEL_VIN4,
    "DIFF_1": pcm1863_input.PCM1863_INPUT_SEL_DIFF_1,
    "DIFF_2": pcm1863_input.PCM1863_INPUT_SEL_DIFF_2,
}

pcm1863_bits_per_sample = pcm1863_ns.enum("PCM1863BitsPerSample")
PCM1863_BITS_PER_SAMPLE_ENUM = {
    16: pcm1863_bits_per_sample.PCM1863_BITS_PER_SAMPLE_16,
    20: pcm1863_bits_per_sample.PCM1863_BITS_PER_SAMPLE_20,
    24: pcm1863_bits_per_sample.PCM1863_BITS_PER_SAMPLE_24,
    32: pcm1863_bits_per_sample.PCM1863_BITS_PER_SAMPLE_32,
}

_validate_bits = cv.float_with_unit("bits", "bit")

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PCM1863),
            cv.Optional(CONF_BITS_PER_SAMPLE, default="24bit"): cv.All(
                _validate_bits, cv.enum(PCM1863_BITS_PER_SAMPLE_ENUM)
            ),
            cv.Optional(CONF_INPUT, default="VIN1"): cv.enum(
                PCM1863_INPUT_ENUM, upper=True
            ),
            cv.Optional(CONF_MIC_GAIN, default="0db"): cv.All(
                cv.decibel,
                cv.float_range(min=PCM1863_MIC_GAIN_MIN, max=PCM1863_MIC_GAIN_MAX),
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x4A))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))
    cg.add(var.set_input(config[CONF_INPUT]))
    cg.add(var.set_mic_gain(config[CONF_MIC_GAIN]))
