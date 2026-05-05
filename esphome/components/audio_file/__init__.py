from dataclasses import dataclass, field
from functools import partial
import hashlib
import logging
from pathlib import Path

import puremagic

from esphome import external_files
import esphome.codegen as cg
from esphome.components import audio
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_ID,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_TYPE,
    CONF_URL,
)
from esphome.core import CORE, ID, HexInt
from esphome.cpp_generator import MockObj
from esphome.external_files import download_web_files_in_config
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@kahrendt"]

AUTO_LOAD = ["audio"]

DOMAIN = "audio_file"

audio_file_ns = cg.esphome_ns.namespace("audio_file")

TYPE_LOCAL = "local"
TYPE_WEB = "web"


@dataclass
class AudioFileData:
    file_ids: dict[str, ID] = field(default_factory=dict)
    file_cache: dict[str, tuple[bytes, MockObj]] = field(default_factory=dict)


def _get_data() -> AudioFileData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = AudioFileData()
    return CORE.data[DOMAIN]


def get_audio_file_ids() -> dict[str, ID]:
    """Get all registered audio file IDs for cross-component access."""
    return _get_data().file_ids


def _compute_local_file_path(value: ConfigType) -> Path:
    url = value[CONF_URL]
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    _LOGGER.debug("_compute_local_file_path: base_dir=%s", base_dir / key)
    return base_dir / key


def _file_schema(value: ConfigType | str) -> ConfigType:
    if isinstance(value, str):
        return _validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


def _validate_file_shorthand(value: str) -> ConfigType:
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


def read_audio_file_and_type(file_config: ConfigType) -> tuple[bytes, MockObj]:
    """Read an audio file and determine its type. Used by this component and media_source platform."""
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

    try:
        file_type: str = puremagic.from_string(data)
        file_type = file_type.removeprefix(".")
    except puremagic.PureError as e:
        raise cv.Invalid(
            f"Unable to determine audio file type of '{path}'. "
            f"Try re-encoding the file into a supported format. Details: {e}"
        ) from e

    media_file_type = audio.AUDIO_FILE_TYPE_ENUM["NONE"]
    if file_type == "wav":
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["WAV"]
    elif file_type in ("mp3", "mpeg", "mpga"):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["MP3"]
    elif file_type == "flac":
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["FLAC"]
    elif (
        file_type == "ogg"
        and len(data) >= 36
        and data.startswith(b"OggS")
        and data[28:36] == b"OpusHead"
    ):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["OPUS"]

    return data, media_file_type


LOCAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): cv.file_,
    }
)

WEB_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_URL): cv.url,
    }
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


MAX_FILE_SIZE = 5 * 1024 * 1024  # 5 MB


def _validate_supported_local_file(config: list[ConfigType]) -> list[ConfigType]:
    for file_config in config:
        data, media_file_type = read_audio_file_and_type(file_config)

        if len(data) > MAX_FILE_SIZE:
            file_info = file_config.get(CONF_FILE, {})
            source = (
                file_info.get(CONF_PATH) or file_info.get(CONF_URL) or "unknown source"
            )
            raise cv.Invalid(
                f"Audio file {source!r} is too large ({len(data)} bytes, max {MAX_FILE_SIZE} bytes)"
            )

        if str(media_file_type) == str(audio.AUDIO_FILE_TYPE_ENUM["NONE"]):
            file_info = file_config.get(CONF_FILE, {})
            source = (
                file_info.get(CONF_PATH) or file_info.get(CONF_URL) or "unknown source"
            )
            raise cv.Invalid(
                f"Unsupported media file from {source!r} (detected type: {media_file_type})"
            )

        # Cache the file data so to_code() doesn't need to re-read it
        _get_data().file_cache[str(file_config[CONF_ID])] = (data, media_file_type)

        media_file_type_str = str(media_file_type)
        if media_file_type_str == str(audio.AUDIO_FILE_TYPE_ENUM["FLAC"]):
            audio.request_flac_support()
        elif media_file_type_str == str(audio.AUDIO_FILE_TYPE_ENUM["MP3"]):
            audio.request_mp3_support()
        elif media_file_type_str == str(audio.AUDIO_FILE_TYPE_ENUM["OPUS"]):
            audio.request_opus_support()
        elif media_file_type_str == str(audio.AUDIO_FILE_TYPE_ENUM["WAV"]):
            audio.request_wav_support()

    return config


CONFIG_SCHEMA = cv.All(
    cv.only_on_esp32,
    cv.ensure_list(MEDIA_FILE_TYPE_SCHEMA),
    partial(download_web_files_in_config, path_for=_compute_local_file_path),
    _validate_supported_local_file,
)


async def to_code(config: list[ConfigType]) -> None:
    cache = _get_data().file_cache

    for file_config in config:
        file_id = str(file_config[CONF_ID])
        data, media_file_type = cache[file_id]

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

        # Store file ID for cross-component access
        _get_data().file_ids[file_id] = file_config[CONF_ID]

    # Register all files in the shared C++ registry
    cg.add_define("AUDIO_FILE_MAX_FILES", len(config))
    for file_config in config:
        file_id = str(file_config[CONF_ID])
        file_var = await cg.get_variable(file_config[CONF_ID])
        cg.add(audio_file_ns.add_named_audio_file(file_var, file_id))
