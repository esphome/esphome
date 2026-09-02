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
    CONF_UPDATE_INTERVAL,
    Framework,
)
from esphome.core import CORE

try:
    from esphome.components.const import CONF_STATE_SAVE_INTERVAL
except ImportError:
    # Support loading this PR as an external component with ESPHome 2026.6.5.
    CONF_STATE_SAVE_INTERVAL = "state_save_interval"

CODEOWNERS = ["@berikv"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor"]
MULTI_CONF = True

DOMAIN = "bme690"

# Bosch BSEC library/config downloads:
# https://www.bosch-sensortec.com/en/software-tools/software-downloads.html?downloadId=bsec3300
# https://www.bosch-sensortec.com/en/products/downloads

# Multiple BME690 instances are supported by configuration, but are currently
# untested. All instances must use the same target-compatible BSEC library,
# because it is linked into the firmware as a single static archive.
CONF_BME690_ID = "bme690_id"
CONF_BSEC_CONFIG = "bsec_config"
CONF_BSEC_LIBRARY = "bsec_library"
KEY_BSEC_LIBRARY = "bsec_library"

bme690_ns = cg.esphome_ns.namespace("bme690")
BME690Component = bme690_ns.class_("BME690Component", cg.Component, i2c.I2CDevice)

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

    if not data:
        raise core.EsphomeError(f"BSEC configuration file {path} is empty")
    # Bosch .config files include a 4-byte little-endian length prefix; the BSEC API expects the payload only.
    if len(data) >= 4 and int.from_bytes(data[:4], "little") == len(data) - 4:
        data = data[4:]
    return list(data)


def _validate_bsec_options(config):
    if CONF_BSEC_CONFIG in config and CONF_BSEC_LIBRARY not in config:
        raise cv.Invalid(f"{CONF_BSEC_CONFIG} requires {CONF_BSEC_LIBRARY}")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BME690Component),
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
            cv.Optional(CONF_BSEC_CONFIG): cv.Any(cv.file_, cv.url),
            cv.Optional(CONF_BSEC_LIBRARY): cv.Any(cv.file_, cv.url),
            cv.Optional(CONF_SAMPLE_RATE, default="LP"): cv.enum(
                SAMPLE_RATE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_TEMPERATURE_OFFSET, default=0): cv.temperature_delta,
            cv.Optional(
                CONF_STATE_SAVE_INTERVAL, default="6hours"
            ): cv.positive_time_period_minutes,
        }
    )
    .extend(
        cv.COMPONENT_SCHEMA.extend(
            {
                cv.Optional(CONF_UPDATE_INTERVAL, default="3s"): cv.update_interval,
            }
        )
    )
    .extend(i2c.i2c_device_schema(0x76)),
    _validate_bsec_options,
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
    state_preference_hash = int(hashlib.sha256(value).hexdigest()[:8], 16)
    cg.add(var.set_state_preference_hash(state_preference_hash))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL].total_milliseconds))

    if (bsec_config := config.get(CONF_BSEC_CONFIG)) is not None:
        config_path = _resolve_bsec_config(bsec_config)
        rhs = _read_bsec_config(config_path)
        config_array = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
        cg.add(var.set_bsec_configuration(config_array, len(rhs)))

    if (bsec_library := config.get(CONF_BSEC_LIBRARY)) is not None:
        cg.add(var.set_bsec_enabled(True))
        lib_path = _resolve_bsec_library(bsec_library)
        data = CORE.data.setdefault(DOMAIN, {})
        if (existing_lib_path := data.get(KEY_BSEC_LIBRARY)) is not None:
            if existing_lib_path != lib_path:
                raise core.EsphomeError(
                    "All BME690 instances must use the same BSEC library archive"
                )
        else:
            data[KEY_BSEC_LIBRARY] = lib_path
            esp32.add_extra_build_file("libalgobsec.a", lib_path)

            cg.add_define("USE_BSEC")
            build_dir = CORE.relative_build_path()
            cg.add_build_flag(
                f"-L{build_dir} -Wl,--whole-archive -lalgobsec -Wl,--no-whole-archive"
            )

    cg.add(
        var.set_state_save_interval(config[CONF_STATE_SAVE_INTERVAL].total_milliseconds)
    )
