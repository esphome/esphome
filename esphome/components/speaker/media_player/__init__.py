"""Speaker Media Player Setup."""

import hashlib
import logging
from pathlib import Path

from esphome import automation, external_files
import esphome.codegen as cg
from esphome.components import audio, esp32, media_player, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_FILE,
    CONF_FILES,
    CONF_FORMAT,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_SAMPLE_RATE,
    CONF_SPEAKER,
    CONF_TASK_STACK_IN_PSRAM,
    CONF_TYPE,
    CONF_URL,
)
from esphome.core import CORE, HexInt
from esphome.core.entity_helpers import inherit_property_from
from esphome.external_files import download_content

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["audio", "psram"]

CODEOWNERS = ["@kahrendt", "@synesthesiam"]
DOMAIN = "media_player"

TYPE_LOCAL = "local"
TYPE_WEB = "web"

CONF_ANNOUNCEMENT = "announcement"
CONF_ANNOUNCEMENT_PIPELINE = "announcement_pipeline"
CONF_CODEC_SUPPORT_ENABLED = "codec_support_enabled"
CONF_ENQUEUE = "enqueue"
CONF_MEDIA_FILE = "media_file"
CONF_MEDIA_PIPELINE = "media_pipeline"
CONF_ON_MUTE = "on_mute"
CONF_ON_UNMUTE = "on_unmute"
CONF_ON_VOLUME = "on_volume"
CONF_STREAM = "stream"
CONF_VOLUME_INCREMENT = "volume_increment"
CONF_VOLUME_MIN = "volume_min"
CONF_VOLUME_MAX = "volume_max"


speaker_ns = cg.esphome_ns.namespace("speaker")
SpeakerMediaPlayer = speaker_ns.class_(
    "SpeakerMediaPlayer",
    media_player.MediaPlayer,
    cg.Component,
)

AudioPipeline = speaker_ns.class_("AudioPipeline")
AudioPipelineType = speaker_ns.enum("AudioPipelineType", is_class=True)
AUDIO_PIPELINE_TYPE_ENUM = {
    "MEDIA": AudioPipelineType.MEDIA,
    "ANNOUNCEMENT": AudioPipelineType.ANNOUNCEMENT,
}

PlayOnDeviceMediaAction = speaker_ns.class_(
    "PlayOnDeviceMediaAction",
    automation.Action,
    cg.Parented.template(SpeakerMediaPlayer),
)
StopStreamAction = speaker_ns.class_(
    "StopStreamAction", automation.Action, cg.Parented.template(SpeakerMediaPlayer)
)


def _compute_local_file_path(value: dict) -> Path:
    url = value[CONF_URL]
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    _LOGGER.debug("_compute_local_file_path: base_dir=%s", base_dir / key)
    return base_dir / key


def _download_web_file(value):
    url = value[CONF_URL]
    path = _compute_local_file_path(value)

    download_content(url, path)
    _LOGGER.debug("download_web_file: path=%s", path)
    return value


# Returns a media_player.MediaPlayerSupportedFormat struct with the configured
# format, sample rate, number of channels, purpose, and bytes per sample
def _get_supported_format_struct(pipeline, type):
    args = [
        media_player.MediaPlayerSupportedFormat,
    ]

    if pipeline[CONF_FORMAT] == "FLAC":
        args.append(("format", "flac"))
    elif pipeline[CONF_FORMAT] == "MP3":
        args.append(("format", "mp3"))
    elif pipeline[CONF_FORMAT] == "WAV":
        args.append(("format", "wav"))

    args.append(("sample_rate", pipeline[CONF_SAMPLE_RATE]))
    args.append(("num_channels", pipeline[CONF_NUM_CHANNELS]))

    if type == "MEDIA":
        args.append(
            (
                "purpose",
                media_player.MEDIA_PLAYER_FORMAT_PURPOSE_ENUM["default"],
            )
        )
    elif type == "ANNOUNCEMENT":
        args.append(
            (
                "purpose",
                media_player.MEDIA_PLAYER_FORMAT_PURPOSE_ENUM["announcement"],
            )
        )
    if pipeline[CONF_FORMAT] != "MP3":
        args.append(("sample_bytes", 2))

    return cg.StructInitializer(*args)


def _file_schema(value):
    if isinstance(value, str):
        return _validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


def _read_audio_file_and_type(file_config):
    conf_file = file_config[CONF_FILE]
    file_source = conf_file[CONF_TYPE]
    if file_source == TYPE_LOCAL:
        path = CORE.relative_config_path(conf_file[CONF_PATH])
    elif file_source == TYPE_WEB:
        path = _compute_local_file_path(conf_file)
    else:
        raise cv.Invalid("Unsupported file source.")

    with open(path, "rb") as f:
        data = f.read()

    import puremagic

    file_type: str = puremagic.from_string(data)
    if file_type.startswith("."):
        file_type = file_type[1:]

    media_file_type = audio.AUDIO_FILE_TYPE_ENUM["NONE"]
    if file_type in ("wav"):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["WAV"]
    elif file_type in ("mp3", "mpeg", "mpga"):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["MP3"]
    elif file_type in ("flac"):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["FLAC"]

    return data, media_file_type


def _validate_file_shorthand(value):
    value = cv.string_strict(value)
    if value.startswith("http://") or value.startswith("https://"):
        return _file_schema(
            {
                CONF_TYPE: TYPE_WEB,
                CONF_URL: value,
            }
        )
    return _file_schema(
        {
            CONF_TYPE: TYPE_LOCAL,
            CONF_PATH: value,
        }
    )


def _validate_pipeline(config):
    # Inherit transcoder settings from speaker if not manually set
    inherit_property_from(CONF_NUM_CHANNELS, CONF_SPEAKER)(config)
    inherit_property_from(CONF_SAMPLE_RATE, CONF_SPEAKER)(config)

    # Validate the transcoder settings is compatible with the speaker
    audio.final_validate_audio_schema(
        "speaker media_player",
        audio_device=CONF_SPEAKER,
        bits_per_sample=16,
        channels=config.get(CONF_NUM_CHANNELS),
        sample_rate=config.get(CONF_SAMPLE_RATE),
    )(config)

    return config


def _validate_repeated_speaker(config):
    if (announcement_config := config.get(CONF_ANNOUNCEMENT_PIPELINE)) and (
        media_config := config.get(CONF_MEDIA_PIPELINE)
    ):
        if announcement_config[CONF_SPEAKER] == media_config[CONF_SPEAKER]:
            raise cv.Invalid(
                "The announcement and media pipelines cannot use the same speaker. Use the `mixer` speaker component to create two source speakers."
            )

    return config


def _validate_supported_local_file(config):
    for file_config in config.get(CONF_FILES, []):
        _, media_file_type = _read_audio_file_and_type(file_config)
        if str(media_file_type) == str(audio.AUDIO_FILE_TYPE_ENUM["NONE"]):
            raise cv.Invalid("Unsupported local media file.")
        if not config[CONF_CODEC_SUPPORT_ENABLED] and str(media_file_type) != str(
            audio.AUDIO_FILE_TYPE_ENUM["WAV"]
        ):
            # Only wav files are supported
            raise cv.Invalid(
                f"Unsupported local media file type, set {CONF_CODEC_SUPPORT_ENABLED} to true or convert the media file to wav"
            )

    return config


LOCAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): cv.file_,
    }
)

WEB_SCHEMA = cv.All(
    {
        cv.Required(CONF_URL): cv.url,
    },
    _download_web_file,
)


TYPED_FILE_SCHEMA = cv.typed_schema(
    {
        TYPE_LOCAL: LOCAL_SCHEMA,
        TYPE_WEB: WEB_SCHEMA,
    },
)


MEDIA_FILE_TYPE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(audio.AudioFile),
        cv.Required(CONF_FILE): _file_schema,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
)

PIPELINE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AudioPipeline),
        cv.Required(CONF_SPEAKER): cv.use_id(speaker.Speaker),
        cv.Optional(CONF_FORMAT, default="FLAC"): cv.enum(audio.AUDIO_FILE_TYPE_ENUM),
        cv.Optional(CONF_SAMPLE_RATE): cv.int_range(min=1),
        cv.Optional(CONF_NUM_CHANNELS): cv.int_range(1, 2),
    }
)

CONFIG_SCHEMA = cv.All(
    media_player.media_player_schema(SpeakerMediaPlayer).extend(
        {
            cv.Required(CONF_ANNOUNCEMENT_PIPELINE): PIPELINE_SCHEMA,
            cv.Optional(CONF_MEDIA_PIPELINE): PIPELINE_SCHEMA,
            cv.Optional(CONF_BUFFER_SIZE, default=1000000): cv.int_range(
                min=4000, max=4000000
            ),
            cv.Optional(CONF_CODEC_SUPPORT_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_FILES): cv.ensure_list(MEDIA_FILE_TYPE_SCHEMA),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM, default=False): cv.boolean,
            cv.Optional(CONF_VOLUME_INCREMENT, default=0.05): cv.percentage,
            cv.Optional(CONF_VOLUME_MAX, default=1.0): cv.percentage,
            cv.Optional(CONF_VOLUME_MIN, default=0.0): cv.percentage,
            cv.Optional(CONF_ON_MUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_UNMUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_VOLUME): automation.validate_automation(single=True),
        }
    ),
    cv.only_with_esp_idf,
    _validate_repeated_speaker,
)


FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_ANNOUNCEMENT_PIPELINE): _validate_pipeline,
            cv.Optional(CONF_MEDIA_PIPELINE): _validate_pipeline,
        },
        extra=cv.ALLOW_EXTRA,
    ),
    _validate_supported_local_file,
)


async def to_code(config):
    if config[CONF_CODEC_SUPPORT_ENABLED]:
        # Compile all supported audio codecs and optimize the wifi settings

        cg.add_define("USE_AUDIO_FLAC_SUPPORT", True)
        cg.add_define("USE_AUDIO_MP3_SUPPORT", True)

        # Wifi settings based on https://github.com/espressif/esp-adf/issues/297#issuecomment-783811702
        esp32.add_idf_sdkconfig_option("CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM", 16)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM", 512)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP32_WIFI_STATIC_TX_BUFFER", True)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP32_WIFI_TX_BUFFER_TYPE", 0)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP32_WIFI_STATIC_TX_BUFFER_NUM", 8)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP32_WIFI_CACHE_TX_BUFFER_NUM", 32)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP32_WIFI_AMPDU_TX_ENABLED", True)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP32_WIFI_TX_BA_WIN", 16)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP32_WIFI_AMPDU_RX_ENABLED", True)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP32_WIFI_RX_BA_WIN", 32)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_MAX_ACTIVE_TCP", 16)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_MAX_LISTENING_TCP", 16)
        esp32.add_idf_sdkconfig_option("CONFIG_TCP_MAXRTX", 12)
        esp32.add_idf_sdkconfig_option("CONFIG_TCP_SYNMAXRTX", 6)
        esp32.add_idf_sdkconfig_option("CONFIG_TCP_MSS", 1436)
        esp32.add_idf_sdkconfig_option("CONFIG_TCP_MSL", 60000)
        esp32.add_idf_sdkconfig_option("CONFIG_TCP_SND_BUF_DEFAULT", 65535)
        esp32.add_idf_sdkconfig_option("CONFIG_TCP_WND_DEFAULT", 512000)
        esp32.add_idf_sdkconfig_option("CONFIG_TCP_RECVMBOX_SIZE", 512)
        esp32.add_idf_sdkconfig_option("CONFIG_TCP_QUEUE_OOSEQ", True)
        esp32.add_idf_sdkconfig_option("CONFIG_TCP_OVERSIZE_MSS", True)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_WND_SCALE", True)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCP_RCV_SCALE", 3)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_RECVMBOX_SIZE", 512)

        # Allocate wifi buffers in PSRAM
        esp32.add_idf_sdkconfig_option("CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP", True)

    var = await media_player.new_media_player(config)
    await cg.register_component(var, config)

    cg.add_define("USE_OTA_STATE_CALLBACK")

    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))

    cg.add(var.set_task_stack_in_psram(config[CONF_TASK_STACK_IN_PSRAM]))
    if config[CONF_TASK_STACK_IN_PSRAM]:
        esp32.add_idf_sdkconfig_option(
            "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
        )

    cg.add(var.set_volume_increment(config[CONF_VOLUME_INCREMENT]))
    cg.add(var.set_volume_max(config[CONF_VOLUME_MAX]))
    cg.add(var.set_volume_min(config[CONF_VOLUME_MIN]))

    announcement_pipeline_config = config[CONF_ANNOUNCEMENT_PIPELINE]
    spkr = await cg.get_variable(announcement_pipeline_config[CONF_SPEAKER])
    cg.add(var.set_announcement_speaker(spkr))
    if announcement_pipeline_config[CONF_FORMAT] != "NONE":
        cg.add(
            var.set_announcement_format(
                _get_supported_format_struct(
                    announcement_pipeline_config, "ANNOUNCEMENT"
                )
            )
        )

    if media_pipeline_config := config.get(CONF_MEDIA_PIPELINE):
        spkr = await cg.get_variable(media_pipeline_config[CONF_SPEAKER])
        cg.add(var.set_media_speaker(spkr))
        if media_pipeline_config[CONF_FORMAT] != "NONE":
            cg.add(
                var.set_media_format(
                    _get_supported_format_struct(media_pipeline_config, "MEDIA")
                )
            )

    if on_mute := config.get(CONF_ON_MUTE):
        await automation.build_automation(
            var.get_mute_trigger(),
            [],
            on_mute,
        )
    if on_unmute := config.get(CONF_ON_UNMUTE):
        await automation.build_automation(
            var.get_unmute_trigger(),
            [],
            on_unmute,
        )
    if on_volume := config.get(CONF_ON_VOLUME):
        await automation.build_automation(
            var.get_volume_trigger(),
            [(cg.float_, "x")],
            on_volume,
        )

    for file_config in config.get(CONF_FILES, []):
        data, media_file_type = _read_audio_file_and_type(file_config)

        rhs = [HexInt(x) for x in data]
        prog_arr = cg.progmem_array(file_config[CONF_RAW_DATA_ID], rhs)

        media_files_struct = cg.StructInitializer(
            audio.AudioFile,
            (
                "data",
                prog_arr,
            ),
            (
                "length",
                len(rhs),
            ),
            (
                "file_type",
                media_file_type,
            ),
        )

        cg.new_Pvariable(
            file_config[CONF_ID],
            media_files_struct,
        )


@automation.register_action(
    "media_player.speaker.play_on_device_media_file",
    PlayOnDeviceMediaAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(SpeakerMediaPlayer),
            cv.Required(CONF_MEDIA_FILE): cv.use_id(audio.AudioFile),
            cv.Optional(CONF_ANNOUNCEMENT, default=False): cv.templatable(cv.boolean),
            cv.Optional(CONF_ENQUEUE, default=False): cv.templatable(cv.boolean),
        },
        key=CONF_MEDIA_FILE,
    ),
)
async def play_on_device_media_media_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    media_file = await cg.get_variable(config[CONF_MEDIA_FILE])
    announcement = await cg.templatable(config[CONF_ANNOUNCEMENT], args, cg.bool_)
    enqueue = await cg.templatable(config[CONF_ENQUEUE], args, cg.bool_)

    cg.add(var.set_audio_file(media_file))
    cg.add(var.set_announcement(announcement))
    cg.add(var.set_enqueue(enqueue))
    return var
