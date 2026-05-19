from esphome import pins
import esphome.codegen as cg
from esphome.components import audio, esp32, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_BUFFER_DURATION,
    CONF_CHANNEL,
    CONF_ID,
    CONF_MODE,
    CONF_NEVER,
    CONF_NUM_CHANNELS,
    CONF_SAMPLE_RATE,
    CONF_TIMEOUT,
)

from .. import (
    CONF_I2S_DOUT_PIN,
    CONF_I2S_MODE,
    CONF_LEFT,
    CONF_MCLK_MULTIPLE,
    CONF_MONO,
    CONF_PRIMARY,
    CONF_RIGHT,
    CONF_STEREO,
    CONF_USE_APLL,
    I2SAudioOut,
    i2s_audio_component_schema,
    i2s_audio_ns,
    register_i2s_audio_component,
    validate_mclk_divisible_by_3,
)

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@jesserockz", "@kahrendt"]
DEPENDENCIES = ["i2s_audio"]

I2SAudioSpeakerBase = i2s_audio_ns.class_(
    "I2SAudioSpeakerBase", cg.Component, speaker.Speaker, I2SAudioOut
)
I2SAudioSpeaker = i2s_audio_ns.class_("I2SAudioSpeaker", I2SAudioSpeakerBase)

CONF_DAC_TYPE = "dac_type"
CONF_I2S_COMM_FMT = "i2s_comm_fmt"
CONF_SPDIF_MODE = "spdif_mode"

I2SAudioSpeakerBase = i2s_audio_ns.class_(
    "I2SAudioSpeakerBase", cg.Component, speaker.Speaker, I2SAudioOut
)
I2SAudioSpeaker = i2s_audio_ns.class_("I2SAudioSpeaker", I2SAudioSpeakerBase)
I2SAudioSpeakerSPDIF = i2s_audio_ns.class_("I2SAudioSpeakerSPDIF", I2SAudioSpeakerBase)

I2SCommFmt = i2s_audio_ns.enum("I2SCommFmt", is_class=True)

I2SCommFmt = i2s_audio_ns.enum("I2SCommFmt", is_class=True)

i2s_dac_mode_t = cg.global_ns.enum("i2s_dac_mode_t")
INTERNAL_DAC_OPTIONS = {
    CONF_LEFT: i2s_dac_mode_t.I2S_DAC_CHANNEL_LEFT_EN,
    CONF_RIGHT: i2s_dac_mode_t.I2S_DAC_CHANNEL_RIGHT_EN,
    CONF_STEREO: i2s_dac_mode_t.I2S_DAC_CHANNEL_BOTH_EN,
}

i2s_comm_format_t = cg.global_ns.enum("i2s_comm_format_t")
I2C_COMM_FMT_OPTIONS = {
    "stand_i2s": i2s_comm_format_t.I2S_COMM_FORMAT_STAND_I2S,
    "stand_msb": i2s_comm_format_t.I2S_COMM_FORMAT_STAND_MSB,
    "stand_pcm_short": i2s_comm_format_t.I2S_COMM_FORMAT_STAND_PCM_SHORT,
    "stand_pcm_long": i2s_comm_format_t.I2S_COMM_FORMAT_STAND_PCM_LONG,
    "stand_max": i2s_comm_format_t.I2S_COMM_FORMAT_STAND_MAX,
    "i2s_msb": i2s_comm_format_t.I2S_COMM_FORMAT_I2S_MSB,
    "i2s_lsb": i2s_comm_format_t.I2S_COMM_FORMAT_I2S_LSB,
    "pcm": i2s_comm_format_t.I2S_COMM_FORMAT_PCM,
    "pcm_short": i2s_comm_format_t.I2S_COMM_FORMAT_PCM_SHORT,
    "pcm_long": i2s_comm_format_t.I2S_COMM_FORMAT_PCM_LONG,
}

INTERNAL_DAC_VARIANTS = [esp32.VARIANT_ESP32]


def _set_num_channels_from_config(config):
    if config[CONF_CHANNEL] in (CONF_MONO, CONF_LEFT, CONF_RIGHT):
        config[CONF_NUM_CHANNELS] = 1
    else:
        config[CONF_NUM_CHANNELS] = 2

    return config


def _set_stream_limits(config):
    if config.get(CONF_SPDIF_MODE, False):
        # SPDIF mode: fixed to 16-bit stereo at configured sample rate
        audio.set_stream_limits(
            min_bits_per_sample=16,
            max_bits_per_sample=16,
            min_channels=2,
            max_channels=2,
            min_sample_rate=config.get(CONF_SAMPLE_RATE),
            max_sample_rate=config.get(CONF_SAMPLE_RATE),
        )(config)
    elif config[CONF_I2S_MODE] == CONF_PRIMARY:
        # Primary mode has modifiable stream settings
        audio.set_stream_limits(
            min_bits_per_sample=8,
            max_bits_per_sample=32,
            min_channels=1,
            max_channels=2,
            min_sample_rate=16000,
            max_sample_rate=48000,
        )(config)
    else:
        # Secondary mode has unmodifiable max bits per sample and min/max sample rates
        audio.set_stream_limits(
            min_bits_per_sample=8,
            max_bits_per_sample=config.get(CONF_BITS_PER_SAMPLE),
            min_channels=1,
            max_channels=2,
            min_sample_rate=config.get(CONF_SAMPLE_RATE),
            max_sample_rate=config.get(CONF_SAMPLE_RATE),
        )

    return config


def _select_speaker_class(config):
    """Override ID type when SPDIF mode is enabled."""
    if config.get(CONF_SPDIF_MODE, False):
        config[CONF_ID].type = I2SAudioSpeakerSPDIF
    return config


def _validate_esp32_variant(config):
    variant = esp32.get_esp32_variant()
    if config[CONF_DAC_TYPE] == "internal":
        if variant not in INTERNAL_DAC_VARIANTS:
            raise cv.Invalid(f"{variant} does not have an internal DAC")
    elif (
        variant == esp32.VARIANT_ESP32
        and config.get(CONF_BITS_PER_SAMPLE) == 8
        and config.get(CONF_CHANNEL) in (CONF_MONO, CONF_LEFT, CONF_RIGHT)
    ):
        raise cv.Invalid("8-bit mono mode is not supported on ESP32")
    return config


BASE_SCHEMA = (
    speaker.SPEAKER_SCHEMA.extend(
        i2s_audio_component_schema(
            I2SAudioSpeaker,
            default_sample_rate=16000,
            default_channel=CONF_MONO,
            default_bits_per_sample="16bit",
        )
    )
    .extend(
        {
            cv.Optional(
                CONF_BUFFER_DURATION, default="500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TIMEOUT, default="500ms"): cv.Any(
                cv.positive_time_period_milliseconds,
                cv.one_of(CONF_NEVER, lower=True),
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "internal": BASE_SCHEMA.extend(
                {
                    cv.Required(CONF_MODE): cv.enum(INTERNAL_DAC_OPTIONS, lower=True),
                }
            ),
            "external": BASE_SCHEMA.extend(
                {
                    cv.Required(
                        CONF_I2S_DOUT_PIN
                    ): pins.internal_gpio_output_pin_number,
                    cv.Optional(CONF_I2S_COMM_FMT, default="stand_i2s"): cv.one_of(
                        *I2C_COMM_FMT_OPTIONS, lower=True
                    ),
                    cv.Optional(CONF_SPDIF_MODE, default=False): cv.boolean,
                }
            ),
        },
        key=CONF_DAC_TYPE,
    ),
    _validate_esp32_variant,
    _set_num_channels_from_config,
    _set_stream_limits,
    _select_speaker_class,
    validate_mclk_divisible_by_3,
)


def _final_validate(config):
    if config[CONF_DAC_TYPE] == "internal":
        raise cv.Invalid(
            "Internal DAC is no longer supported. Use an external I2S DAC instead."
        )
    if config[CONF_I2S_COMM_FMT] == "stand_max":
        raise cv.Invalid("I2S standard max format is no longer supported.")

    if config.get(CONF_SPDIF_MODE, False):
        # SPDIF mode specific validations
        if config[CONF_SAMPLE_RATE] not in [44100, 48000]:
            raise cv.Invalid(
                "SPDIF mode only supports 44100 Hz or 48000 Hz sample rates"
            )
        if config[CONF_CHANNEL] != CONF_STEREO:
            raise cv.Invalid("SPDIF mode only supports stereo channel configuration")
        # bits_per_sample is converted to float by the schema
        if config[CONF_BITS_PER_SAMPLE] != 16:
            raise cv.Invalid("SPDIF mode only supports 16 bits per sample")
        if not config[CONF_USE_APLL]:
            raise cv.Invalid(
                "SPDIF mode requires 'use_apll: true' for accurate clock generation"
            )
        if config[CONF_I2S_MODE] != CONF_PRIMARY:
            raise cv.Invalid("SPDIF mode requires 'i2s_mode: primary'")
        if config[CONF_I2S_COMM_FMT] != "stand_i2s":
            raise cv.Invalid("SPDIF mode requires 'i2s_comm_fmt: stand_i2s'")
        if config[CONF_MCLK_MULTIPLE] != 256:
            raise cv.Invalid("SPDIF mode requires 'mclk_multiple: 256'")


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_i2s_audio_component(var, config)
    await speaker.register_speaker(var, config)

    cg.add(var.set_dout_pin(config[CONF_I2S_DOUT_PIN]))

    is_spdif = config.get(CONF_SPDIF_MODE, False)
    if is_spdif:
        cg.add_define("USE_I2S_AUDIO_SPDIF_MODE")
    else:
        fmt = I2SCommFmt.STANDARD  # equals stand_i2s, stand_pcm_long, i2s_msb, pcm_long
        if config[CONF_I2S_COMM_FMT] in ["stand_msb", "i2s_lsb"]:
            fmt = I2SCommFmt.MSB
        elif config[CONF_I2S_COMM_FMT] in ["stand_pcm_short", "pcm_short", "pcm"]:
            fmt = I2SCommFmt.PCM
        cg.add(var.set_i2s_comm_fmt(fmt))

    if config[CONF_TIMEOUT] != CONF_NEVER:
        cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_buffer_duration(config[CONF_BUFFER_DURATION]))
