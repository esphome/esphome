"""ESPHome codegen for the gsl3670 touchscreen sub-platform."""

import logging
import pathlib
from pathlib import Path

from esphome import pins
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
)
from esphome.core import ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["touchscreen"]
LOGGER = logging.getLogger(__name__)


gsl3670_ns = cg.esphome_ns.namespace("gsl3670")
GSL3670Touchscreen = gsl3670_ns.class_(
    "GSL3670Touchscreen",
    touchscreen.Touchscreen,
    i2c.I2CDevice,
)

CONF_FIRMWARE_FILE = "firmware_file"

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
        CONF_FIRMWARE_FILE: "seeed-d1001-fw.bin",
    },
    "CUSTOM": {},
}


def _validate_firmware_file(value):
    cv.string_strict(value)
    path = pathlib.Path(value)
    if not path.exists():
        path = Path(__file__).parent / path.name
    if not path.exists():
        raise cv.Invalid(f"Firmware file not found: {path.absolute()}")
    with path.open("rb") as f:
        data = f.read()
    blk_cnt = len(data) // _FW_BLK_SIZE
    if blk_cnt * _FW_BLK_SIZE != len(data):
        raise cv.Invalid(f"Firmware file length is incorrect: {path.absolute()}")
    for i in range(0, len(data), _FW_BLK_SIZE):
        if data[i] > 0xEF or data[i + 1] != 1 or data[i + 2] != 2 or data[i + 3] != 3:
            raise cv.Invalid(
                f"Corrupted firmware at block {i // _FW_BLK_SIZE} in file: {path.absolute()}"
            )
    return value


def _config_schema(config):
    model_option = {
        cv.Optional(CONF_MODEL, default="CUSTOM"): cv.one_of(*MODELS, upper=True)
    }
    config = cv.Schema(model_option, extra=True)(config)
    defaults = MODELS[config[CONF_MODEL]]
    return (
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
                    CONF_FIRMWARE_FILE, defaults, required=True
                ): _validate_firmware_file,
            }
        )
        .extend(i2c.i2c_device_schema(0x40))
        .extend(cv.COMPONENT_SCHEMA)(config)
    )


CONFIG_SCHEMA = _config_schema

_FW_BLK_SIZE = 128 + 4


def _read_firmware(config) -> bytes:
    path = pathlib.Path(config[CONF_FIRMWARE_FILE])
    if not path.exists():
        path = Path(__file__).parent / path.name
    with path.open("rb") as f:
        data = f.read()
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
