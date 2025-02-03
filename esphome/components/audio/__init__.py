import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_BITS_PER_SAMPLE, CONF_NUM_CHANNELS, CONF_SAMPLE_RATE
import esphome.final_validate as fv

CODEOWNERS = ["@kahrendt"]
audio_ns = cg.esphome_ns.namespace("audio")

AudioFile = audio_ns.struct("AudioFile")
AudioFileType = audio_ns.enum("AudioFileType", is_class=True)
AUDIO_FILE_TYPE_ENUM = {
    "NONE": AudioFileType.NONE,
    "WAV": AudioFileType.WAV,
    "MP3": AudioFileType.MP3,
    "FLAC": AudioFileType.FLAC,
}


CONF_MIN_BITS_PER_SAMPLE = "min_bits_per_sample"
CONF_MAX_BITS_PER_SAMPLE = "max_bits_per_sample"
CONF_MIN_CHANNELS = "min_channels"
CONF_MAX_CHANNELS = "max_channels"
CONF_MIN_SAMPLE_RATE = "min_sample_rate"
CONF_MAX_SAMPLE_RATE = "max_sample_rate"


CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
)

AUDIO_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_BITS_PER_SAMPLE): cv.int_range(8, 32),
        cv.Optional(CONF_NUM_CHANNELS): cv.int_range(1, 2),
        cv.Optional(CONF_SAMPLE_RATE): cv.int_range(8000, 48000),
    }
)


_UNDEF = object()


def set_stream_limits(
    min_bits_per_sample: int = _UNDEF,
    max_bits_per_sample: int = _UNDEF,
    min_channels: int = _UNDEF,
    max_channels: int = _UNDEF,
    min_sample_rate: int = _UNDEF,
    max_sample_rate: int = _UNDEF,
):
    def set_limits_in_config(config):
        if min_bits_per_sample is not _UNDEF:
            config[CONF_MIN_BITS_PER_SAMPLE] = min_bits_per_sample
        if max_bits_per_sample is not _UNDEF:
            config[CONF_MAX_BITS_PER_SAMPLE] = max_bits_per_sample
        if min_channels is not _UNDEF:
            config[CONF_MIN_CHANNELS] = min_channels
        if max_channels is not _UNDEF:
            config[CONF_MAX_CHANNELS] = max_channels
        if min_sample_rate is not _UNDEF:
            config[CONF_MIN_SAMPLE_RATE] = min_sample_rate
        if max_sample_rate is not _UNDEF:
            config[CONF_MAX_SAMPLE_RATE] = max_sample_rate

    return set_limits_in_config


def final_validate_audio_schema(
    name: str,
    *,
    audio_device: str,
    bits_per_sample: int,
    channels: int,
    sample_rate: int,
):
    def validate_audio_compatiblity(audio_config):
        audio_schema = {}

        try:
            cv.int_range(
                min=audio_config.get(CONF_MIN_BITS_PER_SAMPLE),
                max=audio_config.get(CONF_MAX_BITS_PER_SAMPLE),
            )(bits_per_sample)
        except cv.Invalid as exc:
            raise cv.Invalid(
                f"Invalid configuration for the {name} component. The {CONF_BITS_PER_SAMPLE} {str(exc)}"
            ) from exc

        try:
            cv.int_range(
                min=audio_config.get(CONF_MIN_CHANNELS),
                max=audio_config.get(CONF_MAX_CHANNELS),
            )(channels)
        except cv.Invalid as exc:
            raise cv.Invalid(
                f"Invalid configuration for the {name} component. The {CONF_NUM_CHANNELS} {str(exc)}"
            ) from exc

        try:
            cv.int_range(
                min=audio_config.get(CONF_MIN_SAMPLE_RATE),
                max=audio_config.get(CONF_MAX_SAMPLE_RATE),
            )(sample_rate)
            return cv.Schema(audio_schema, extra=cv.ALLOW_EXTRA)(audio_config)
        except cv.Invalid as exc:
            raise cv.Invalid(
                f"Invalid configuration for the {name} component. The {CONF_SAMPLE_RATE} {str(exc)}"
            ) from exc

    return cv.Schema(
        {
            cv.Required(audio_device): fv.id_declaration_match_schema(
                validate_audio_compatiblity
            )
        },
        extra=cv.ALLOW_EXTRA,
    )


async def to_code(config):
    cg.add_library("esphome/esp-audio-libs", "1.1.1")
