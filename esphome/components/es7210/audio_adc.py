import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_adc import AudioAdc
import esphome.config_validation as cv
from esphome.const import CONF_BITS_PER_SAMPLE, CONF_ID, CONF_MIC_GAIN, CONF_SAMPLE_RATE

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["i2c"]

es7210_ns = cg.esphome_ns.namespace("es7210")
ES7210 = es7210_ns.class_("ES7210", AudioAdc, cg.Component, i2c.I2CDevice)


es7210_bits_per_sample = es7210_ns.enum("ES7210BitsPerSample")
ES7210_BITS_PER_SAMPLE_ENUM = {
    16: es7210_bits_per_sample.ES7210_BITS_PER_SAMPLE_16,
    24: es7210_bits_per_sample.ES7210_BITS_PER_SAMPLE_24,
    32: es7210_bits_per_sample.ES7210_BITS_PER_SAMPLE_32,
}


ES7210_MIC_GAINS = [0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 34.5, 36, 37.5]

_validate_bits = cv.float_with_unit("bits", "bit")

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES7210),
            cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): cv.All(
                _validate_bits, cv.enum(ES7210_BITS_PER_SAMPLE_ENUM)
            ),
            cv.Optional(CONF_MIC_GAIN, default="24db"): cv.All(
                cv.decibel, cv.one_of(*ES7210_MIC_GAINS)
            ),
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(min=1),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x40))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))
    cg.add(var.set_mic_gain(config[CONF_MIC_GAIN]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
