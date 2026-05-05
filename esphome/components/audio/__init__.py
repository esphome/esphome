from dataclasses import dataclass, field

import esphome.codegen as cg
from esphome.components.esp32 import (
    add_idf_component,
    add_idf_sdkconfig_option,
    include_builtin_idf_component,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_NUM_CHANNELS,
    CONF_SAMPLE_RATE,
    CONF_SIZE,
)
from esphome.core import CORE
import esphome.final_validate as fv

CODEOWNERS = ["@kahrendt"]
DOMAIN = "audio"
audio_ns = cg.esphome_ns.namespace("audio")

AudioFile = audio_ns.struct("AudioFile")
AudioFileType = audio_ns.enum("AudioFileType", is_class=True)
AUDIO_FILE_TYPE_ENUM = {
    "NONE": AudioFileType.NONE,
    "WAV": AudioFileType.WAV,
    "MP3": AudioFileType.MP3,
    "FLAC": AudioFileType.FLAC,
    "OPUS": AudioFileType.OPUS,
}

MEMORY_PSRAM = "psram"
MEMORY_INTERNAL = "internal"
MEMORY_LOCATIONS = [MEMORY_PSRAM, MEMORY_INTERNAL]


@dataclass
class FlacOptions:
    buffer_memory: str | None = None


@dataclass
class Mp3Options:
    buffer_memory: str | None = None


@dataclass
class OpusPseudostackOptions:
    threadsafe: bool | None = None
    buffer_memory: str | None = None
    size: int | None = None


@dataclass
class OpusOptions:
    floating_point: bool | None = None
    state_memory: str | None = None
    pseudostack: OpusPseudostackOptions = field(default_factory=OpusPseudostackOptions)


@dataclass
class AudioData:
    flac_support: bool = False
    mp3_support: bool = False
    opus_support: bool = False
    wav_support: bool = False
    micro_decoder_support: bool = False
    flac: FlacOptions = field(default_factory=FlacOptions)
    mp3: Mp3Options = field(default_factory=Mp3Options)
    opus: OpusOptions = field(default_factory=OpusOptions)


def _get_data() -> AudioData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = AudioData()
    return CORE.data[DOMAIN]


def request_flac_support() -> None:
    """Request FLAC codec support for audio decoding."""
    _get_data().flac_support = True


def request_mp3_support() -> None:
    """Request MP3 codec support for audio decoding."""
    _get_data().mp3_support = True


def request_opus_support() -> None:
    """Request Opus codec support for audio decoding."""
    _get_data().opus_support = True


def request_wav_support() -> None:
    """Request WAV codec support for audio decoding."""
    _get_data().wav_support = True


def request_micro_decoder_support() -> None:
    """Request micro-decoder library support for audio decoding."""
    _get_data().micro_decoder_support = True


CONF_MIN_BITS_PER_SAMPLE = "min_bits_per_sample"
CONF_MAX_BITS_PER_SAMPLE = "max_bits_per_sample"
CONF_MIN_CHANNELS = "min_channels"
CONF_MAX_CHANNELS = "max_channels"
CONF_MIN_SAMPLE_RATE = "min_sample_rate"
CONF_MAX_SAMPLE_RATE = "max_sample_rate"

CONF_CODECS = "codecs"
CONF_WAV = "wav"
CONF_FLAC = "flac"
CONF_MP3 = "mp3"
CONF_OPUS = "opus"
CONF_BUFFER_MEMORY = "buffer_memory"
CONF_FLOATING_POINT = "floating_point"
CONF_STATE_MEMORY = "state_memory"
CONF_PSEUDOSTACK = "pseudostack"
CONF_THREADSAFE = "threadsafe"


_MEMORY_LOCATION_VALIDATOR = cv.one_of(*MEMORY_LOCATIONS, lower=True)


def _maybe_empty_codec(schema):
    """Wrap a codec dict schema so that a bare key (None value) is treated as an empty dict."""

    def validator(value):
        if value is None:
            value = {}
        return schema(value)

    return validator


CODEC_FLAC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_BUFFER_MEMORY): _MEMORY_LOCATION_VALIDATOR,
    }
)

CODEC_MP3_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_BUFFER_MEMORY): _MEMORY_LOCATION_VALIDATOR,
    }
)

OPUS_PSEUDOSTACK_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_THREADSAFE): cv.boolean,
        cv.Optional(CONF_BUFFER_MEMORY): _MEMORY_LOCATION_VALIDATOR,
        cv.Optional(CONF_SIZE): cv.int_range(60000, 240000),
    }
)

CODEC_OPUS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FLOATING_POINT): cv.boolean,
        cv.Optional(CONF_STATE_MEMORY): _MEMORY_LOCATION_VALIDATOR,
        cv.Optional(CONF_PSEUDOSTACK): _maybe_empty_codec(OPUS_PSEUDOSTACK_SCHEMA),
    }
)

CODEC_WAV_SCHEMA = cv.Schema({})

CODECS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FLAC): _maybe_empty_codec(CODEC_FLAC_SCHEMA),
        cv.Optional(CONF_MP3): _maybe_empty_codec(CODEC_MP3_SCHEMA),
        cv.Optional(CONF_OPUS): _maybe_empty_codec(CODEC_OPUS_SCHEMA),
        cv.Optional(CONF_WAV): _maybe_empty_codec(CODEC_WAV_SCHEMA),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_CODECS): _maybe_empty_codec(CODECS_SCHEMA),
        }
    ),
    cv.only_on_esp32,
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


def _emit_memory_pair(value: str | None, psram_key: str, internal_key: str) -> None:
    if value == MEMORY_PSRAM:
        add_idf_sdkconfig_option(psram_key, True)
        add_idf_sdkconfig_option(internal_key, False)
    elif value == MEMORY_INTERNAL:
        add_idf_sdkconfig_option(psram_key, False)
        add_idf_sdkconfig_option(internal_key, True)


async def to_code(config):
    # Re-enable ESP-IDF's HTTP client (excluded by default to save compile time)
    include_builtin_idf_component("esp_http_client")

    add_idf_component(
        name="esphome/esp-audio-libs",
        ref="2.0.4",
    )

    data = _get_data()

    # Merge user-supplied codec configuration (additive: presence enables the codec)
    if codecs_config := config.get(CONF_CODECS):
        if (flac_config := codecs_config.get(CONF_FLAC)) is not None:
            data.flac_support = True
            if (buffer_memory := flac_config.get(CONF_BUFFER_MEMORY)) is not None:
                data.flac.buffer_memory = buffer_memory
        if (mp3_config := codecs_config.get(CONF_MP3)) is not None:
            data.mp3_support = True
            if (buffer_memory := mp3_config.get(CONF_BUFFER_MEMORY)) is not None:
                data.mp3.buffer_memory = buffer_memory
        if (opus_config := codecs_config.get(CONF_OPUS)) is not None:
            data.opus_support = True
            floating_point = opus_config.get(CONF_FLOATING_POINT)
            if floating_point is not None:
                data.opus.floating_point = floating_point
            if (state_memory := opus_config.get(CONF_STATE_MEMORY)) is not None:
                data.opus.state_memory = state_memory
            if (pseudostack_config := opus_config.get(CONF_PSEUDOSTACK)) is not None:
                threadsafe = pseudostack_config.get(CONF_THREADSAFE)
                if threadsafe is not None:
                    data.opus.pseudostack.threadsafe = threadsafe
                if (
                    buffer_memory := pseudostack_config.get(CONF_BUFFER_MEMORY)
                ) is not None:
                    data.opus.pseudostack.buffer_memory = buffer_memory
                if (size := pseudostack_config.get(CONF_SIZE)) is not None:
                    data.opus.pseudostack.size = size
        if CONF_WAV in codecs_config:
            data.wav_support = True

    if data.micro_decoder_support:
        add_idf_component(name="esphome/micro-decoder", ref="0.2.0")

        # All codecs are enabled by default in micro-decoder, so disable the ones that aren't requested to save flash
        if not data.flac_support:
            add_idf_sdkconfig_option("CONFIG_MICRO_DECODER_CODEC_FLAC", False)
        if not data.mp3_support:
            add_idf_sdkconfig_option("CONFIG_MICRO_DECODER_CODEC_MP3", False)
        if not data.opus_support:
            add_idf_sdkconfig_option("CONFIG_MICRO_DECODER_CODEC_OPUS", False)
        if not data.wav_support:
            add_idf_sdkconfig_option("CONFIG_MICRO_DECODER_CODEC_WAV", False)

    # Configure each codec library.
    # Adds a define and IDF component for legacy `audio_decoder.cpp`.
    if data.flac_support:
        cg.add_define("USE_AUDIO_FLAC_SUPPORT")
        add_idf_component(name="esphome/micro-flac", ref="0.1.1")
        _emit_memory_pair(
            data.flac.buffer_memory,
            "CONFIG_MICRO_FLAC_PREFER_PSRAM",
            "CONFIG_MICRO_FLAC_PREFER_INTERNAL",
        )
    if data.mp3_support:
        cg.add_define("USE_AUDIO_MP3_SUPPORT")
        add_idf_component(name="esphome/micro-mp3", ref="0.2.0")
        _emit_memory_pair(
            data.mp3.buffer_memory,
            "CONFIG_MP3_DECODER_PREFER_PSRAM",
            "CONFIG_MP3_DECODER_PREFER_INTERNAL",
        )
    if data.opus_support:
        cg.add_define("USE_AUDIO_OPUS_SUPPORT")
        add_idf_component(name="esphome/micro-opus", ref="0.4.1")
        if data.opus.floating_point is not None:
            add_idf_sdkconfig_option(
                "CONFIG_OPUS_FLOATING_POINT", data.opus.floating_point
            )
        _emit_memory_pair(
            data.opus.state_memory,
            "CONFIG_OPUS_STATE_PREFER_PSRAM",
            "CONFIG_OPUS_STATE_PREFER_INTERNAL",
        )
        if data.opus.pseudostack.threadsafe is True:
            add_idf_sdkconfig_option("CONFIG_OPUS_THREADSAFE_PSEUDOSTACK", True)
            add_idf_sdkconfig_option("CONFIG_OPUS_NONTHREADSAFE_PSEUDOSTACK", False)
        elif data.opus.pseudostack.threadsafe is False:
            add_idf_sdkconfig_option("CONFIG_OPUS_THREADSAFE_PSEUDOSTACK", False)
            add_idf_sdkconfig_option("CONFIG_OPUS_NONTHREADSAFE_PSEUDOSTACK", True)
        _emit_memory_pair(
            data.opus.pseudostack.buffer_memory,
            "CONFIG_OPUS_PSEUDOSTACK_PREFER_PSRAM",
            "CONFIG_OPUS_PSEUDOSTACK_PREFER_INTERNAL",
        )
        if data.opus.pseudostack.size is not None:
            add_idf_sdkconfig_option(
                "CONFIG_OPUS_PSEUDOSTACK_SIZE", data.opus.pseudostack.size
            )
    if data.wav_support:
        cg.add_define("USE_AUDIO_WAV_SUPPORT")
        add_idf_component(name="esphome/micro-wav", ref="0.2.0")
