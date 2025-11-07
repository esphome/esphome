"""Binary Storage Component - Unified interface for FRAM, EEPROM, Flash storage devices."""
from __future__ import annotations

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_MODEL, CONF_TYPE
import re

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = []
MULTI_CONF = True

# Namespace
binary_storage_ns = cg.esphome_ns.namespace("binary_storage")

# Base class
BinaryStorage = binary_storage_ns.class_("BinaryStorage", cg.Component)

# I2C EEPROM class
I2CEeprom = binary_storage_ns.class_("I2CEeprom", BinaryStorage, i2c.I2CDevice)

# Configuration keys
CONF_PAGE_SIZE = "page_size"
CONF_CAPACITY = "capacity"
CONF_ADDRESSING_BITS = "addressing_bits"


def validate_bytes(value):
    """Validate and parse byte size with units (e.g., '32KB', '256KiB')."""
    value = cv.string(value).lower()
    match = re.match(r"^([0-9]+)\s*(\w*)$", value)

    if match is None:
        raise cv.Invalid(f"Expected number with optional unit, got {value}")

    suffixes = {
        "": 1,
        "b": 1,
        "kb": 1000,
        "kib": 1024,
        "mb": 1000 ** 2,
        "mib": 1024 ** 2,
    }

    suffix = match.group(2)
    if suffix and suffix not in suffixes:
        raise cv.Invalid(f"Invalid suffix '{suffix}', valid: B, KB, KiB, MB, MiB")

    return int(int(match.group(1)) * suffixes[suffix])


# EEPROM Configuration Schema
EEPROM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(I2CEeprom),
            cv.Optional(CONF_MODEL, default="AT24C256"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_PAGE_SIZE): cv.int_range(min=8, max=128),
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(8, 9, 10, 11, 16, int=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x50))
)

# Typed schema for device selection
CONFIG_SCHEMA = cv.typed_schema(
    {
        "EEPROM": EEPROM_SCHEMA,
        "I2C_EEPROM": EEPROM_SCHEMA,
    },
    key=CONF_TYPE,
    default_type="EEPROM",
    upper=True,
)


async def to_code(config):
    """Configure binary storage device."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # Set model name
    if CONF_MODEL in config:
        cg.add(var.set_model(config[CONF_MODEL]))

    # Set capacity if specified (otherwise auto-detected from model)
    if CONF_CAPACITY in config:
        cg.add(var.set_capacity(config[CONF_CAPACITY]))

    # Set page size if specified
    if CONF_PAGE_SIZE in config:
        cg.add(var.set_page_size(config[CONF_PAGE_SIZE]))

    # Set addressing bits if specified
    if CONF_ADDRESSING_BITS in config:
        cg.add(var.set_addressing_bits(config[CONF_ADDRESSING_BITS]))

    # Register with storage_host via CORE.data
    from esphome.core import CORE

    if not hasattr(CORE, "data"):
        CORE.data = {}
    if "binary_storage_devices" not in CORE.data:
        CORE.data["binary_storage_devices"] = []
    CORE.data["binary_storage_devices"].append(var)
