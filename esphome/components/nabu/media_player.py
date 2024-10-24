"""Nabu Media Player Setup."""

import hashlib
import logging
from pathlib import Path

from esphome import automation, external_files
import esphome.codegen as cg
from esphome.components import esp32, media_player, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_DURATION,
    CONF_FILE,
    CONF_FILES,
    CONF_ID,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_SAMPLE_RATE,
    CONF_SPEAKER,
    CONF_TYPE,
    CONF_URL,
)
from esphome.core import CORE, HexInt
from esphome.external_files import download_content

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["audio", "psram"]

CODEOWNERS = ["@synesthesiam", "@kahrendt"]
DEPENDENCIES = ["media_player"]
DOMAIN = "file"

TYPE_LOCAL = "local"
TYPE_WEB = "web"

CONF_DECIBEL_REDUCTION = "decibel_reduction"

CONF_ANNOUNCEMENT = "announcement"
CONF_MEDIA_FILE = "media_file"
CONF_PIPELINE = "pipeline"
CONF_VOLUME_INCREMENT = "volume_increment"
CONF_VOLUME_MIN = "volume_min"
CONF_VOLUME_MAX = "volume_max"

CONF_ON_MUTE = "on_mute"
CONF_ON_UNMUTE = "on_unmute"
CONF_ON_VOLUME = "on_volume"

nabu_ns = cg.esphome_ns.namespace("nabu")
NabuMediaPlayer = nabu_ns.class_("NabuMediaPlayer")
NabuMediaPlayer = nabu_ns.class_(
    "NabuMediaPlayer",
    NabuMediaPlayer,
    media_player.MediaPlayer,
    cg.Component,
)

MediaFile = nabu_ns.struct("MediaFile")
MediaFileType = nabu_ns.enum("MediaFileType", is_class=True)
MEDIA_FILE_TYPE_ENUM = {
    "NONE": MediaFileType.NONE,
    "WAV": MediaFileType.WAV,
    "MP3": MediaFileType.MP3,
    "FLAC": MediaFileType.FLAC,
}

PipelineType = nabu_ns.enum("AudioPipelineType", is_class=True)
PIPELINE_TYPE_ENUM = {
    "MEDIA": PipelineType.MEDIA,
    "ANNOUNCEMENT": PipelineType.ANNOUNCEMENT,
}

PlayLocalMediaAction = nabu_ns.class_(
    "PlayLocalMediaAction", automation.Action, cg.Parented.template(NabuMediaPlayer)
)
StopPipelineAction = nabu_ns.class_(
    "StopPipelineAction", automation.Action, cg.Parented.template(NabuMediaPlayer)
)
DuckingSetAction = nabu_ns.class_(
    "DuckingSetAction", automation.Action, cg.Parented.template(NabuMediaPlayer)
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

    try:
        import puremagic

        file_type: str = puremagic.from_string(data)
    except ImportError:
        try:
            from magic import Magic

            magic = Magic(mime=True)
            file_type: str = magic.from_buffer(data)
        except ImportError as exc:
            raise cv.Invalid("Please install puremagic") from exc
    if file_type.startswith("."):
        file_type = file_type[1:]

    media_file_type = MEDIA_FILE_TYPE_ENUM["NONE"]
    if file_type in ("wav"):
        media_file_type = MEDIA_FILE_TYPE_ENUM["WAV"]
    elif file_type in ("mp3", "mpeg", "mpga"):
        media_file_type = MEDIA_FILE_TYPE_ENUM["MP3"]
    elif file_type in ("flac"):
        media_file_type = MEDIA_FILE_TYPE_ENUM["FLAC"]

    return data, media_file_type


def _supported_local_file_validate(config):
    if files_list := config.get(CONF_FILES):
        for file_config in files_list:
            _, media_file_type = _read_audio_file_and_type(file_config)
            if str(media_file_type) == str(MEDIA_FILE_TYPE_ENUM["NONE"]):
                raise cv.Invalid("Unsupported local media file.")


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
        cv.Required(CONF_ID): cv.declare_id(MediaFile),
        cv.Required(CONF_FILE): _file_schema,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
)


CONFIG_SCHEMA = cv.All(
    media_player.MEDIA_PLAYER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(NabuMediaPlayer),
            cv.Required(CONF_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(min=1),
            cv.Optional(CONF_VOLUME_INCREMENT, default=0.05): cv.percentage,
            cv.Optional(CONF_VOLUME_MAX, default=1.0): cv.percentage,
            cv.Optional(CONF_VOLUME_MIN, default=0.0): cv.percentage,
            cv.Optional(CONF_FILES): cv.ensure_list(MEDIA_FILE_TYPE_SCHEMA),
            cv.Optional(CONF_ON_MUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_UNMUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_VOLUME): automation.validate_automation(single=True),
        }
    ),
    cv.only_with_esp_idf,
)
FINAL_VALIDATE_SCHEMA = _supported_local_file_validate


async def to_code(config):
    cg.add_library("https://github.com/esphome/esp-audio-libs", "1.0.0")

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
    esp32.add_idf_sdkconfig_option("CONFIG_TCP_SND_BUF_DEFAULT", 5840)
    esp32.add_idf_sdkconfig_option(
        "CONFIG_TCP_WND_DEFAULT", 65535
    )  # Adjusted from referenced settings to avoid compilation error
    esp32.add_idf_sdkconfig_option("CONFIG_TCP_RECVMBOX_SIZE", 512)
    esp32.add_idf_sdkconfig_option("CONFIG_TCP_QUEUE_OOSEQ", True)
    esp32.add_idf_sdkconfig_option("CONFIG_TCP_OVERSIZE_MSS", True)
    esp32.add_idf_sdkconfig_option("CONFIG_LWIP_WND_SCALE", True)
    esp32.add_idf_sdkconfig_option("CONFIG_TCP_RCV_SCALE", 3)
    esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_RECVMBOX_SIZE", 512)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_player.register_media_player(var, config)

    cg.add_define("USE_OTA_STATE_CALLBACK")

    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))

    cg.add(var.set_volume_increment(config[CONF_VOLUME_INCREMENT]))
    cg.add(var.set_volume_max(config[CONF_VOLUME_MAX]))
    cg.add(var.set_volume_min(config[CONF_VOLUME_MIN]))

    spkr = await cg.get_variable(config[CONF_SPEAKER])
    cg.add(var.set_speaker(spkr))

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

    if files_list := config.get(CONF_FILES):
        for file_config in files_list:
            data, media_file_type = _read_audio_file_and_type(file_config)

            rhs = [HexInt(x) for x in data]
            prog_arr = cg.progmem_array(file_config[CONF_RAW_DATA_ID], rhs)

            media_files_struct = cg.StructInitializer(
                MediaFile,
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
    "nabu.play_local_media_file",
    PlayLocalMediaAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(NabuMediaPlayer),
            cv.Required(CONF_MEDIA_FILE): cv.use_id(MediaFile),
            cv.Optional(CONF_ANNOUNCEMENT, default=False): cv.boolean,
        },
        key=CONF_MEDIA_FILE,
    ),
)
async def nabu_play_local_media_media_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    media_file = await cg.get_variable(config[CONF_MEDIA_FILE])
    cg.add(var.set_media_file(media_file))
    cg.add(var.set_announcement(config[CONF_ANNOUNCEMENT]))
    return var


@automation.register_action(
    "nabu.stop_pipeline",
    StopPipelineAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(NabuMediaPlayer),
            cv.Required(CONF_PIPELINE): cv.enum(PIPELINE_TYPE_ENUM, upper=True),
        },
        key=CONF_PIPELINE,
    ),
)
async def nabu_stop_pipeline_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_pipeline_type(config[CONF_PIPELINE]))
    return var


@automation.register_action(
    "nabu.set_ducking",
    DuckingSetAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(NabuMediaPlayer),
            cv.Required(CONF_DECIBEL_REDUCTION): cv.templatable(
                cv.int_range(min=0, max=51)
            ),
            cv.Optional(CONF_DURATION, default="0.0s"): cv.templatable(
                cv.positive_time_period_seconds
            ),
        }
    ),
)
async def ducking_set_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    decibel_reduction = await cg.templatable(
        config[CONF_DECIBEL_REDUCTION], args, cg.uint8
    )
    cg.add(var.set_decibel_reduction(decibel_reduction))
    duration = await cg.templatable(config[CONF_DURATION], args, cg.float_)
    cg.add(var.set_duration(duration))
    return var
