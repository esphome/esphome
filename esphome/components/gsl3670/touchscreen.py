"""ESPHome codegen for the gsl3670 touchscreen sub-platform."""

import hashlib
import logging
from pathlib import Path

from esphome import external_files, pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
from esphome.components.const import CONF_SHA256
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
    CONF_FILE,
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

CONF_FIRMWARE = "firmware"

# Firmware blobs are published as release assets of the companion repository
# rather than vendored into the ESPHome source tree. The default URL/SHA-256
# for each model point at a pinned release artifact; users may override them
# (or supply a local file via `firmware: { file: ... }`).
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
        CONF_FIRMWARE: {
            CONF_URL: f"{FIRMWARE_BASE_URL}/seeed-d1001-fw.bin",
            CONF_SHA256: "2e50501ad83656fb6fa3d92591f9f31add4d442c8e8a79f29f5c4d335bd127a4",
        },
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


def _cache_path(url: str) -> Path:
    """Cache path for a downloaded firmware blob, keyed by URL."""
    key = hashlib.sha256(url.encode()).hexdigest()[:8]
    return external_files.compute_local_file_dir(DOMAIN) / key


def firmware_path(firmware: dict) -> Path:
    """Return the path the firmware bytes will be read from at codegen time."""
    if path := firmware.get(CONF_FILE):
        return path
    return _cache_path(firmware[CONF_URL])


def _validate_firmware(firmware: dict) -> dict:
    """Require a single source, download (with caching), verify and validate."""
    if (CONF_FILE in firmware) == (CONF_URL in firmware):
        raise cv.Invalid(
            f"Exactly one of '{CONF_URL}' or '{CONF_FILE}' must be provided"
        )

    if path := firmware.get(CONF_FILE):
        _validate_firmware_data(path.read_bytes(), str(path.absolute()))
        return firmware

    url = firmware[CONF_URL]
    data = external_files.download_content(url, _cache_path(url))

    if expected := firmware.get(CONF_SHA256):
        actual = hashlib.sha256(data).hexdigest()
        if actual.lower() != expected.lower():
            raise cv.Invalid(
                f"Firmware SHA-256 mismatch for {url}: "
                f"expected {expected.lower()}, got {actual}",
                [CONF_SHA256],
            )
    else:
        LOGGER.warning(
            "No SHA256 provided for gsl3670 firmware - firmware integrity can not be checked"
        )
    _validate_firmware_data(data, url)
    return firmware


FIRMWARE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_URL): cv.url,
            cv.Optional(CONF_SHA256): cv.string_strict,
            cv.Optional(CONF_FILE): cv.file_,
        }
    ),
    _validate_firmware,
)


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
                option_with_default(
                    CONF_FIRMWARE, defaults, required=True
                ): FIRMWARE_SCHEMA,
            }
        )
        .extend(i2c.i2c_device_schema(0x40))
        .extend(cv.COMPONENT_SCHEMA)
    )
    return schema(config)


CONFIG_SCHEMA = _config_schema


def _read_firmware(config) -> bytes:
    path = firmware_path(config[CONF_FIRMWARE])
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
