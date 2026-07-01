from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_ENABLE_PIN,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
)

CODEOWNERS = ["@remcom"]
DEPENDENCIES = ["i2c"]

CONF_ANALOG_GAIN = "analog_gain"
CONF_CHANNEL_MIX = "channel_mix"
CONF_VOLUME_MIN_DB = "volume_min_db"
CONF_VOLUME_MAX_DB = "volume_max_db"

pcm5122_ns = cg.esphome_ns.namespace("pcm5122")
PCM5122 = pcm5122_ns.class_("PCM5122", AudioDac, cg.Component, i2c.I2CDevice)
CONF_PCM5122 = "pcm5122"

pcm5122_bits_per_sample = pcm5122_ns.enum("PCM5122BitsPerSample")
PCM5122_BITS_PER_SAMPLE_ENUM = {
    16: pcm5122_bits_per_sample.PCM5122_BITS_PER_SAMPLE_16,
    24: pcm5122_bits_per_sample.PCM5122_BITS_PER_SAMPLE_24,
    32: pcm5122_bits_per_sample.PCM5122_BITS_PER_SAMPLE_32,
}

pcm5122_analog_gain = pcm5122_ns.enum("PCM5122AnalogGain")
PCM5122_ANALOG_GAIN_ENUM = {
    "0db": pcm5122_analog_gain.PCM5122_ANALOG_GAIN_0DB,
    "-6db": pcm5122_analog_gain.PCM5122_ANALOG_GAIN_MINUS_6DB,
}

pcm5122_channel_mix = pcm5122_ns.enum("PCM5122ChannelMix")
PCM5122_CHANNEL_MIX_ENUM = {
    "stereo": pcm5122_channel_mix.PCM5122_CHANNEL_MIX_STEREO,
    "left": pcm5122_channel_mix.PCM5122_CHANNEL_MIX_LEFT_ONLY,
    "right": pcm5122_channel_mix.PCM5122_CHANNEL_MIX_RIGHT_ONLY,
    "swapped": pcm5122_channel_mix.PCM5122_CHANNEL_MIX_SWAPPED,
}

_validate_bits = cv.float_with_unit("bits", "bit")


def _validate_volume_range(config):
    if config[CONF_VOLUME_MIN_DB] >= config[CONF_VOLUME_MAX_DB]:
        raise cv.Invalid(f"{CONF_VOLUME_MIN_DB} must be less than {CONF_VOLUME_MAX_DB}")
    return config


PCM5122GPIOPin = pcm5122_ns.class_(
    "PCM5122GPIOPin",
    cg.GPIOPin,
    cg.Parented.template(PCM5122),
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PCM5122),
            cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): cv.All(
                _validate_bits, cv.enum(PCM5122_BITS_PER_SAMPLE_ENUM)
            ),
            cv.Optional(CONF_ANALOG_GAIN, default="0db"): cv.enum(
                PCM5122_ANALOG_GAIN_ENUM, lower=True
            ),
            cv.Optional(CONF_CHANNEL_MIX, default="stereo"): cv.enum(
                PCM5122_CHANNEL_MIX_ENUM, lower=True
            ),
            cv.Optional(CONF_VOLUME_MIN_DB, default="-52.5dB"): cv.All(
                cv.decibel, cv.float_range(min=-103.0, max=24.0)
            ),
            cv.Optional(CONF_VOLUME_MAX_DB, default="0dB"): cv.All(
                cv.decibel, cv.float_range(min=-103.0, max=24.0)
            ),
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x4D)),
    _validate_volume_range,
)


def _validate_pin_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output, not both")
    return value


def _validate_pin(value):
    if value[CONF_MODE][CONF_INPUT] and value[CONF_NUMBER] == 6:
        raise cv.Invalid("GPIO6 cannot be used as input on the PCM5122")
    return value


PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        PCM5122GPIOPin,
        cv.int_range(min=3, max=6),
        modes=[CONF_INPUT, CONF_OUTPUT],
        mode_validator=_validate_pin_mode,
    ).extend(
        {
            cv.Required(CONF_PCM5122): cv.use_id(PCM5122),
        }
    ),
    _validate_pin,
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_PCM5122, PIN_SCHEMA)
async def pcm5122_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_PCM5122])

    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))
    cg.add(var.set_analog_gain(config[CONF_ANALOG_GAIN]))
    cg.add(var.set_channel_mix(config[CONF_CHANNEL_MIX]))
    cg.add(var.set_volume_min_db(config[CONF_VOLUME_MIN_DB]))
    cg.add(var.set_volume_max_db(config[CONF_VOLUME_MAX_DB]))
    if enable_pin_config := config.get(CONF_ENABLE_PIN):
        enable_pin = await cg.gpio_pin_expression(enable_pin_config)
        cg.add(var.set_enable_pin(enable_pin))
