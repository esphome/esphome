import hashlib
from pathlib import Path
import urllib.parse

from esphome import core, external_files
import esphome.codegen as cg
from esphome.components import esp32, i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_RAW_DATA_ID,
    CONF_SAMPLE_RATE,
    CONF_TEMPERATURE_OFFSET,
    Framework,
)
from esphome.core import CORE

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor"]
MULTI_CONF = True

DOMAIN = "bme690"

# Multiple BME690 instances are supported by configuration, but are currently
# untested. All instances must use the same target-compatible BSEC library,
# because it is linked into the firmware as a single static archive.
CONF_BME690_ID = "bme690_id"
CONF_BSEC_CONFIG = "bsec_config"
CONF_BSEC_LIBRARY = "bsec_library"
CONF_STATE_SAVE_INTERVAL = "state_save_interval"

bme690_ns = cg.esphome_ns.namespace("bme690")
BME690Component = bme690_ns.class_(
    "BME690Component", cg.PollingComponent, i2c.I2CDevice
)

SampleRate = bme690_ns.enum("SampleRate")
SAMPLE_RATE_OPTIONS = {
    "LP": SampleRate.SAMPLE_RATE_LP,
    "ULP": SampleRate.SAMPLE_RATE_ULP,
}


def _compute_local_file_path(url: str, suffix: str) -> Path:
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    return base_dir / f"bsec_{key}{suffix}"


def _resolve_external_file(value: Path | str, suffix: str) -> Path:
    if isinstance(value, Path):
        return value

    parsed = urllib.parse.urlparse(value)
    if parsed.scheme == "file":
        file_path = Path(parsed.path)
        if not file_path.is_file():
            raise cv.Invalid(f"Could not find file '{file_path}'")
        return file_path

    path = _compute_local_file_path(value, suffix)
    external_files.download_content(value, path)
    return path


def _resolve_bsec_library(value: Path | str) -> Path:
    return _resolve_external_file(value, ".a")


def _resolve_bsec_config(value: Path | str) -> Path:
    return _resolve_external_file(value, ".config")


def _read_bsec_config(path: Path) -> list[int]:
    try:
        data = path.read_bytes()
    except Exception as e:
        raise core.EsphomeError(
            f"Could not open BSEC configuration file {path}: {e}"
        ) from e

    # Bosch ships BSEC3 configs as binary .config files. Keep support for
    # comma-separated byte lists too, matching the BSEC2 component convention.
    if b"\0" in data:
        return list(data)

    try:
        text = data.decode("utf-8").strip()
    except UnicodeDecodeError:
        return list(data)

    if not text:
        raise core.EsphomeError(f"BSEC configuration file {path} is empty")

    allowed_chars = set("0123456789abcdefABCDEFxX, \t\r\n")
    if any(char not in allowed_chars for char in text):
        raise core.EsphomeError(
            f"BSEC configuration file {path} must be a binary .config file or "
            "a comma-separated byte list"
        )

    try:
        values = [int(value.strip(), 0) for value in text.split(",") if value.strip()]
    except ValueError as e:
        raise core.EsphomeError(
            f"Could not parse BSEC configuration file {path}: {e}"
        ) from e

    if not values:
        raise core.EsphomeError(f"BSEC configuration file {path} is empty")
    if any(value < 0 or value > 0xFF for value in values):
        raise core.EsphomeError(
            f"BSEC configuration file {path} contains values outside the byte range"
        )
    return values


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BME690Component),
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
            cv.Optional(CONF_BSEC_CONFIG): cv.Any(cv.file_, cv.url),
            cv.Required(CONF_BSEC_LIBRARY): cv.Any(cv.file_, cv.url),
            cv.Optional(CONF_SAMPLE_RATE, default="LP"): cv.enum(
                SAMPLE_RATE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_TEMPERATURE_OFFSET, default=0): cv.temperature_delta,
            cv.Optional(
                CONF_STATE_SAVE_INTERVAL, default="6hours"
            ): cv.positive_time_period_minutes,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x76)),
    cv.only_with_framework(
        frameworks=Framework.ESP_IDF,
        suggestions={
            Framework.ARDUINO: ("bme680", "bme68x_bsec2"),
        },
    ),
    cv.only_on_esp32,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    value = config[CONF_ID].id
    if isinstance(value, str):
        value = value.encode()
    state_preference_hash = int(hashlib.md5(value).hexdigest()[:8], 16)
    cg.add(var.set_state_preference_hash(state_preference_hash))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))

    if (bsec_config := config.get(CONF_BSEC_CONFIG)) is not None:
        config_path = _resolve_bsec_config(bsec_config)
        rhs = _read_bsec_config(config_path)
        config_array = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
        cg.add(var.set_bsec_configuration(config_array, len(rhs)))

    lib_path = _resolve_bsec_library(config[CONF_BSEC_LIBRARY])
    esp32.add_extra_build_file("libalgobsec.a", lib_path)

    build_dir = CORE.relative_build_path()
    cg.add_build_flag(
        f"-L{build_dir} -Wl,--whole-archive -lalgobsec -Wl,--no-whole-archive"
    )

    cg.add(
        var.set_state_save_interval(config[CONF_STATE_SAVE_INTERVAL].total_milliseconds)
    )
