import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_BITS_PER_SAMPLE, CONF_ID, CONF_MIC_GAIN, CONF_SAMPLE_RATE

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["i2c"]

es7210_ns = cg.esphome_ns.namespace("es7210")
ES7210 = es7210_ns.class_("ES7210", cg.Component, i2c.I2CDevice)


es7210_bits_per_sample = es7210_ns.enum("ES7210BitsPerSample")
ES7210_BITS_PER_SAMPLE_ENUM = {
    16: es7210_bits_per_sample.ES7210_BITS_PER_SAMPLE_16,
    24: es7210_bits_per_sample.ES7210_BITS_PER_SAMPLE_24,
    32: es7210_bits_per_sample.ES7210_BITS_PER_SAMPLE_32,
}


es7210_mic_gain = es7210_ns.enum("ES7210MicGain")
ES7210_MIC_GAIN_ENUM = {
    "0DB": es7210_mic_gain.ES7210_MIC_GAIN_0DB,
    "3DB": es7210_mic_gain.ES7210_MIC_GAIN_3DB,
    "6DB": es7210_mic_gain.ES7210_MIC_GAIN_6DB,
    "9DB": es7210_mic_gain.ES7210_MIC_GAIN_9DB,
    "12DB": es7210_mic_gain.ES7210_MIC_GAIN_12DB,
    "15DB": es7210_mic_gain.ES7210_MIC_GAIN_15DB,
    "18DB": es7210_mic_gain.ES7210_MIC_GAIN_18DB,
    "21DB": es7210_mic_gain.ES7210_MIC_GAIN_21DB,
    "24DB": es7210_mic_gain.ES7210_MIC_GAIN_24DB,
    "27DB": es7210_mic_gain.ES7210_MIC_GAIN_27DB,
    "30DB": es7210_mic_gain.ES7210_MIC_GAIN_30DB,
    "33DB": es7210_mic_gain.ES7210_MIC_GAIN_33DB,
    "34.5DB": es7210_mic_gain.ES7210_MIC_GAIN_34_5DB,
    "36DB": es7210_mic_gain.ES7210_MIC_GAIN_36DB,
    "37.5DB": es7210_mic_gain.ES7210_MIC_GAIN_37_5DB,
}

_validate_bits = cv.float_with_unit("bits", "bit")

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES7210),
            cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): cv.All(
                _validate_bits, cv.enum(ES7210_BITS_PER_SAMPLE_ENUM)
            ),
            cv.Optional(CONF_MIC_GAIN, default="24DB"): cv.enum(
                ES7210_MIC_GAIN_ENUM, upper=True
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
