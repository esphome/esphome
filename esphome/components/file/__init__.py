import hashlib
import logging
from pathlib import Path

from magic import Magic

from esphome import external_files
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_FORMAT,
    CONF_ID,
    CONF_PATH,
    CONF_TYPE,
    CONF_URL,
)
from esphome.core import CORE, HexInt
from esphome.external_files import download_content

_LOGGER = logging.getLogger(__name__)


CODEOWNERS = ["@jesserockz"]
DOMAIN = "file"
MULTI_CONF = True

TYPE_LOCAL = "local"
TYPE_WEB = "web"

FORMAT_RAW = "raw"
FORMAT_WAV = "wav"

FORMATS = [FORMAT_RAW, FORMAT_WAV]


def _compute_local_file_path(value: dict) -> Path:
    url = value[CONF_URL]
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    _LOGGER.debug("_compute_local_file_path: base_dir=%s", base_dir / key)
    return base_dir / key


def _download_web_file(value: dict) -> dict:
    url = value[CONF_URL]
    path = _compute_local_file_path(value)

    download_content(url, path)
    _LOGGER.debug("download_web_file: path=%s", path)
    return value


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


TYPED_FILE_SCHEMA = cv.typed_schema(
    {
        TYPE_LOCAL: LOCAL_SCHEMA,
        TYPE_WEB: WEB_SCHEMA,
    },
)


def _file_schema(value):
    if isinstance(value, str):
        return _validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(cg.uint8),
        cv.Required(CONF_FILE): _file_schema,
        cv.Optional(CONF_FORMAT): cv.one_of(*FORMATS, lower=True),
    }
)


def _trim_wav_file(data: bytes) -> bytes:
    header = []
    index = 0
    length = len(data)
    while index < length:
        byte = data[index : index + 1]
        if byte == b"":
            raise ValueError("Could not find data in wav file")
        header.append(byte)
        index += 1
        if header[-4:] == [b"d", b"a", b"t", b"a"] or index > 100:
            break
    index += 2
    return data[index:]


async def to_code(config: dict) -> None:
    conf_file: dict = config[CONF_FILE]
    file_source = conf_file[CONF_TYPE]
    if file_source == TYPE_LOCAL:
        path = CORE.relative_config_path(conf_file[CONF_PATH])
    elif file_source == TYPE_WEB:
        path = _compute_local_file_path(conf_file)

    with open(path, "rb") as f:
        data = f.read()

    # Get format from config or fallback to magic
    if (format := config.get(CONF_FORMAT)) is None:
        magic = Magic(mime=True)
        file_type = magic.from_buffer(data)
        if "wav" in file_type:
            format = FORMAT_WAV

    if format == FORMAT_WAV:
        data = _trim_wav_file(data)

    rhs = [HexInt(x) for x in data]
    cg.progmem_array(config[CONF_ID], rhs)
