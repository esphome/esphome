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


def set_stream_limits(
    min_bits_per_sample: int = cv.UNDEFINED,
    max_bits_per_sample: int = cv.UNDEFINED,
    min_channels: int = cv.UNDEFINED,
    max_channels: int = cv.UNDEFINED,
    min_sample_rate: int = cv.UNDEFINED,
    max_sample_rate: int = cv.UNDEFINED,
):
    """Sets the limits for the audio stream that audio component can handle

    When the component sinks audio (e.g., a speaker), these indicate the limits to the audio it can receive.
    When the component sources audio (e.g., a microphone), these indicate the limits to the audio it can send.
    """

    def set_limits_in_config(config):
        if min_bits_per_sample is not cv.UNDEFINED:
            config[CONF_MIN_BITS_PER_SAMPLE] = min_bits_per_sample
        if max_bits_per_sample is not cv.UNDEFINED:
            config[CONF_MAX_BITS_PER_SAMPLE] = max_bits_per_sample
        if min_channels is not cv.UNDEFINED:
            config[CONF_MIN_CHANNELS] = min_channels
        if max_channels is not cv.UNDEFINED:
            config[CONF_MAX_CHANNELS] = max_channels
        if min_sample_rate is not cv.UNDEFINED:
            config[CONF_MIN_SAMPLE_RATE] = min_sample_rate
        if max_sample_rate is not cv.UNDEFINED:
            config[CONF_MAX_SAMPLE_RATE] = max_sample_rate

    return set_limits_in_config


def final_validate_audio_schema(
    name: str,
    *,
    audio_device: str,
    bits_per_sample: int = cv.UNDEFINED,
    channels: int = cv.UNDEFINED,
    sample_rate: int = cv.UNDEFINED,
    enabled_channels: list[int] = cv.UNDEFINED,
    audio_device_issue: bool = False,
):
    """Validates audio compatibility when passed between different components.

    The component derived from ``AUDIO_COMPONENT_SCHEMA`` should call ``set_stream_limits`` in a validator to specify its compatible settings

      - If audio_device_issue is True, then the error message indicates the user should adjust the AUDIO_COMPONENT_SCHEMA derived component's configuration to match the values passed to this function
      - If audio_device_issue is False, then the error message indicates the user should adjust the configuration of the component calling this function, as it falls out of the valid stream limits

    Args:
        name (str): Friendly name of the component calling this function with an audio component to validate
        audio_device (str): The configuration parameter name that contains the ID of an AUDIO_COMPONENT_SCHEMA derived component to validate against
        bits_per_sample (int, optional): The desired bits per sample
        channels (int, optional): The desired number of channels
        sample_rate (int, optional): The desired sample rate
        enabled_channels (list[int], optional): The desired enabled channels
        audio_device_issue (bool, optional): Format the error message to indicate the problem is in the configuration for the ``audio_device`` component. Defaults to False.
    """

    def validate_audio_compatiblity(audio_config):
        audio_schema = {}

        if bits_per_sample is not cv.UNDEFINED:
            try:
                cv.int_range(
                    min=audio_config.get(CONF_MIN_BITS_PER_SAMPLE),
                    max=audio_config.get(CONF_MAX_BITS_PER_SAMPLE),
                )(bits_per_sample)
            except cv.Invalid as exc:
                if audio_device_issue:
                    error_string = f"Invalid configuration for the specified {audio_device}. The {name} component requires {bits_per_sample} bits per sample."
                else:
                    error_string = f"Invalid configuration for the {name} component. The {CONF_BITS_PER_SAMPLE} {str(exc)}"
                raise cv.Invalid(error_string) from exc

        if channels is not cv.UNDEFINED:
            try:
                cv.int_range(
                    min=audio_config.get(CONF_MIN_CHANNELS),
                    max=audio_config.get(CONF_MAX_CHANNELS),
                )(channels)
            except cv.Invalid as exc:
                if audio_device_issue:
                    error_string = f"Invalid configuration for the specified {audio_device}. The {name} component requires {channels} channels."
                else:
                    error_string = f"Invalid configuration for the {name} component. The {CONF_NUM_CHANNELS} {str(exc)}"
                raise cv.Invalid(error_string) from exc

        if sample_rate is not cv.UNDEFINED:
            try:
                cv.int_range(
                    min=audio_config.get(CONF_MIN_SAMPLE_RATE),
                    max=audio_config.get(CONF_MAX_SAMPLE_RATE),
                )(sample_rate)
            except cv.Invalid as exc:
                if audio_device_issue:
                    error_string = f"Invalid configuration for the specified {audio_device}. The {name} component requires a {sample_rate} sample rate."
                else:
                    error_string = f"Invalid configuration for the {name} component. The {CONF_SAMPLE_RATE} {str(exc)}"
                raise cv.Invalid(error_string) from exc

        if enabled_channels is not cv.UNDEFINED:
            for channel in enabled_channels:
                try:
                    # Channels are 0-indexed
                    cv.int_range(
                        min=0,
                        max=audio_config.get(CONF_MAX_CHANNELS) - 1,
                    )(channel)
                except cv.Invalid as exc:
                    if audio_device_issue:
                        error_string = f"Invalid configuration for the specified {audio_device}. The {name} component requires channel {channel}."
                    else:
                        error_string = f"Invalid configuration for the {name} component. Enabled channel {channel} {str(exc)}"
                    raise cv.Invalid(error_string) from exc

        return cv.Schema(audio_schema, extra=cv.ALLOW_EXTRA)(audio_config)

    return cv.Schema(
        {
            cv.Required(audio_device): fv.id_declaration_match_schema(
                validate_audio_compatiblity
            )
        },
        extra=cv.ALLOW_EXTRA,
    )


async def to_code(config):
    cg.add_library("esphome/esp-audio-libs", "1.1.4")
