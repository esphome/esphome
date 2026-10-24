import esphome.codegen as cg
from esphome.components import esp32, i2c
from esphome.components.audio_adc import AudioAdc
from esphome.components.esp32.const import VARIANT_ESP32, VARIANT_ESP32S2
import esphome.config_validation as cv
from esphome.const import CONF_BITS_PER_SAMPLE, CONF_MIC_GAIN, CONF_SAMPLE_RATE
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["i2c"]

CONF_CHANNEL_GAINS = "channel_gains"
CONF_TDM = "tdm"

es7210_ns = cg.esphome_ns.namespace("es7210")
ES7210 = es7210_ns.class_("ES7210", AudioAdc, cg.Component, i2c.I2CDevice)


es7210_bits_per_sample = es7210_ns.enum("ES7210BitsPerSample")
ES7210_BITS_PER_SAMPLE_ENUM = {
    16: es7210_bits_per_sample.ES7210_BITS_PER_SAMPLE_16,
    24: es7210_bits_per_sample.ES7210_BITS_PER_SAMPLE_24,
    32: es7210_bits_per_sample.ES7210_BITS_PER_SAMPLE_32,
}


ES7210_MIC_GAINS = [0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 34.5, 36, 37.5]
ES7210_MIC_GAIN_DEFAULT = 24

_validate_bits = cv.float_with_unit("bits", "bit")


def _validate_tdm(value: bool) -> bool:
    """Reject TDM on ESP32 variants without TDM hardware support."""
    if value and CORE.is_esp32:
        esp32.only_on_variant(
            unsupported=[VARIANT_ESP32, VARIANT_ESP32S2],
            msg_prefix="ES7210 TDM mode",
        )(value)
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES7210),
            cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): cv.All(
                _validate_bits, cv.enum(ES7210_BITS_PER_SAMPLE_ENUM)
            ),
            cv.Optional(CONF_MIC_GAIN): cv.All(
                cv.decibel, cv.one_of(*ES7210_MIC_GAINS)
            ),
            cv.Optional(CONF_CHANNEL_GAINS): cv.All(
                cv.ensure_list(cv.All(cv.decibel, cv.one_of(*ES7210_MIC_GAINS))),
                cv.Length(min=4, max=4),
            ),
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(min=1),
            cv.Optional(CONF_TDM, default=False): cv.All(cv.boolean, _validate_tdm),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x40)),
    cv.has_at_most_one_key(CONF_MIC_GAIN, CONF_CHANNEL_GAINS),
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))
    if channel_gains := config.get(CONF_CHANNEL_GAINS):
        for channel, gain in enumerate(channel_gains):
            cg.add(var.set_channel_gain(channel, gain))
    else:
        cg.add(var.set_mic_gain(config.get(CONF_MIC_GAIN, ES7210_MIC_GAIN_DEFAULT)))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_tdm(config[CONF_TDM]))
