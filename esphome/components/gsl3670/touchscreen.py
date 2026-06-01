"""ESPHome codegen for the gsl3670 touchscreen sub-platform."""

import hashlib
import logging
from pathlib import Path

from esphome import external_files, pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
from esphome.components.touchscreen import (
    CONF_X_MAX,
    CONF_X_MIN,
    CONF_Y_MAX,
    CONF_Y_MIN,
    option_with_default,
    touchscreen_schema,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INTERRUPT_PIN,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_RESET_PIN,
    CONF_SWAP_XY,
    CONF_URL,
)
from esphome.core import ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["touchscreen"]
LOGGER = logging.getLogger(__name__)

DOMAIN = "gsl3670"

gsl3670_ns = cg.esphome_ns.namespace("gsl3670")
GSL3670Touchscreen = gsl3670_ns.class_(
    "GSL3670Touchscreen",
    touchscreen.Touchscreen,
    i2c.I2CDevice,
)

CONF_FIRMWARE_FILE = "firmware_file"
CONF_SHA256 = "sha256"

# Firmware blobs are published as release assets of the companion repository
# rather than vendored into the ESPHome source tree. The default URL/SHA-256
# for each model point at a pinned release artifact; users may override them
# (or supply a local file via `firmware_file`).
FIRMWARE_RELEASE = "v1.0.0"
FIRMWARE_BASE_URL = f"https://github.com/esphome-libs/gsl3670-firmware/releases/download/{FIRMWARE_RELEASE}"

MODELS = {
    "SEEED-RETERMINAL-D1001": {
        CONF_SWAP_XY: True,
        CONF_MIRROR_X: True,
        CONF_MIRROR_Y: True,
        CONF_X_MIN: 20,
        CONF_Y_MIN: 20,
        CONF_X_MAX: 872,
        CONF_Y_MAX: 1644,
        CONF_RESET_PIN: {"xl9535": None, "number": 14},
        CONF_INTERRUPT_PIN: 16,
        CONF_URL: f"{FIRMWARE_BASE_URL}/seeed-d1001-fw.bin",
        CONF_SHA256: "2e50501ad83656fb6fa3d92591f9f31add4d442c8e8a79f29f5c4d335bd127a4",
    },
    "CUSTOM": {},
}

_FW_BLK_SIZE = 128 + 4


def _validate_firmware_data(data: bytes, source: str) -> None:
    """Validate the structure of a decoded GSL3670 firmware blob."""
    blk_cnt = len(data) // _FW_BLK_SIZE
    if blk_cnt == 0 or blk_cnt * _FW_BLK_SIZE != len(data):
        raise cv.Invalid(f"Firmware file length is incorrect: {source}")
    for i in range(0, len(data), _FW_BLK_SIZE):
        if data[i] > 0xEF or data[i + 1] != 1 or data[i + 2] != 2 or data[i + 3] != 3:
            raise cv.Invalid(
                f"Corrupted firmware at block {i // _FW_BLK_SIZE} in: {source}"
            )


def _local_firmware_path(value: str) -> Path:
    """Resolve a `firmware_file` value to an existing local path."""
    path = Path(value)
    if not path.exists():
        path = Path(__file__).parent / path.name
    return path


def _cache_path(url: str) -> Path:
    """Cache path for a downloaded firmware blob, keyed by URL."""
    key = hashlib.sha256(url.encode()).hexdigest()[:8]
    return external_files.compute_local_file_dir(DOMAIN) / key


def firmware_path(config: dict) -> Path:
    """Return the path the firmware bytes will be read from at codegen time."""
    if CONF_FIRMWARE_FILE in config:
        return _local_firmware_path(config[CONF_FIRMWARE_FILE])
    return _cache_path(config[CONF_URL])


def _download_and_validate_firmware(config: dict) -> dict:
    """Download (with caching) or read the firmware, verify and validate it."""
    if CONF_FIRMWARE_FILE in config:
        path = _local_firmware_path(config[CONF_FIRMWARE_FILE])
        if not path.exists():
            raise cv.Invalid(
                f"Firmware file not found: {path.absolute()}", [CONF_FIRMWARE_FILE]
            )
        data = path.read_bytes()
        _validate_firmware_data(data, str(path.absolute()))
        return config

    url = config[CONF_URL]
    path = _cache_path(url)
    data = external_files.download_content(url, path)

    if expected := config.get(CONF_SHA256):
        actual = hashlib.sha256(data).hexdigest()
        if actual.lower() != expected.lower():
            raise cv.Invalid(
                f"Firmware SHA-256 mismatch for {url}: "
                f"expected {expected.lower()}, got {actual}",
                [CONF_SHA256],
            )
    _validate_firmware_data(data, url)
    return config


def _require_firmware_source(config: dict) -> dict:
    if CONF_FIRMWARE_FILE not in config and CONF_URL not in config:
        raise cv.Invalid(
            f"Either '{CONF_URL}' or '{CONF_FIRMWARE_FILE}' must be provided "
            f"(or use a known '{CONF_MODEL}' that supplies a default URL)"
        )
    return config


def _config_schema(config):
    model_option = {
        cv.Optional(CONF_MODEL, default="CUSTOM"): cv.one_of(*MODELS, upper=True)
    }
    config = cv.Schema(model_option, extra=True)(config)
    defaults = MODELS[config[CONF_MODEL]]
    schema = (
        touchscreen_schema(cv.UNDEFINED, False, defaults)
        .extend(
            {
                cv.GenerateID(): cv.declare_id(GSL3670Touchscreen),
                option_with_default(
                    CONF_INTERRUPT_PIN, defaults
                ): pins.internal_gpio_input_pin_schema,
                option_with_default(
                    CONF_RESET_PIN, defaults
                ): pins.gpio_output_pin_schema,
                **model_option,
                option_with_default(CONF_URL, defaults): cv.url,
                option_with_default(CONF_SHA256, defaults): cv.string_strict,
                cv.Optional(CONF_FIRMWARE_FILE): cv.string_strict,
            }
        )
        .extend(i2c.i2c_device_schema(0x40))
        .extend(cv.COMPONENT_SCHEMA)
        .add_extra(_require_firmware_source)
        .add_extra(_download_and_validate_firmware)
    )
    return schema(config)


CONFIG_SCHEMA = _config_schema


def _read_firmware(config) -> bytes:
    path = firmware_path(config)
    data = path.read_bytes()
    LOGGER.info(
        "Read gsl3670 touchscreen firmware file %s: %d bytes, %d blocks",
        path.absolute(),
        len(data),
        len(data) // _FW_BLK_SIZE,
    )
    return data


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_INTERRUPT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
        cg.add(var.set_interrupt_pin(pin))

    if CONF_RESET_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(pin))

    #  Firmware table
    data = _read_firmware(config)
    fw_array = cg.progmem_array(
        ID(config[CONF_ID].id + "_fw", type=cg.uint8), list(data)
    )
    cg.add(var.set_firmware(fw_array, len(data) // _FW_BLK_SIZE))
