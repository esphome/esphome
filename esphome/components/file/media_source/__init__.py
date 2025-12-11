import hashlib
import logging
from pathlib import Path

from esphome import external_files
import esphome.codegen as cg
from esphome.components import audio, media_source
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_FILES,
    CONF_ID,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_TYPE,
    CONF_URL,
)
from esphome.core import CORE, HexInt
from esphome.external_files import download_content

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["media_source", "audio"]
DOMAIN = "file_media_source"

TYPE_LOCAL = "local"
TYPE_WEB = "web"

file_ns = cg.esphome_ns.namespace("file")
FileMediaSource = file_ns.class_(
    "FileMediaSource", cg.Component, media_source.MediaSource
)

CONF_TASK_STACK_IN_PSRAM = "task_stack_in_psram"


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


def _file_schema(value):
    if isinstance(value, str):
        return _validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


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


def _read_audio_file_and_type(file_config):
    conf_file = file_config[CONF_FILE]
    file_source = conf_file[CONF_TYPE]
    if file_source == TYPE_LOCAL:
        path = CORE.relative_config_path(conf_file[CONF_PATH])
    elif file_source == TYPE_WEB:
        path = _compute_local_file_path(conf_file)
    else:
        raise cv.Invalid("Unsupported file source")

    with open(path, "rb") as f:
        data = f.read()

    import puremagic

    file_type: str = puremagic.from_string(data)
    file_type = file_type.removeprefix(".")

    media_file_type = audio.AUDIO_FILE_TYPE_ENUM["NONE"]
    if file_type in ("wav"):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["WAV"]
    elif file_type in ("mp3", "mpeg", "mpga"):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["MP3"]
    elif file_type in ("flac"):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["FLAC"]

    return data, media_file_type


def _validate_supported_local_file(config):
    for file_config in config.get(CONF_FILES, []):
        _, media_file_type = _read_audio_file_and_type(file_config)
        if str(media_file_type) == str(audio.AUDIO_FILE_TYPE_ENUM["NONE"]):
            raise cv.Invalid("Unsupported local media file")

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

CONFIG_SCHEMA = (
    media_source.media_source_schema(
        FileMediaSource,
        media_player=False,
    )
    .extend(
        {
            cv.Optional(CONF_FILES): cv.ensure_list(MEDIA_FILE_TYPE_SCHEMA),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


FINAL_VALIDATE_SCHEMA = _validate_supported_local_file


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_source.register_media_source(var, config)

    if CONF_TASK_STACK_IN_PSRAM in config:
        cg.add(var.set_task_stack_in_psram(config[CONF_TASK_STACK_IN_PSRAM]))

    cg.add(var.set_uri_prefix("file"))

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

        file_var = cg.new_Pvariable(
            file_config[CONF_ID],
            media_files_struct,
        )

        # Add the file to the media source's file vector
        file_id = str(file_config[CONF_ID])
        cg.add(var.add_file(file_var, file_id))
