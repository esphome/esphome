"""Binary Storage Component - Unified interface for FRAM, EEPROM, Flash storage devices."""
from __future__ import annotations

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = []
MULTI_CONF = True

# Namespace
binary_storage_ns = cg.esphome_ns.namespace("binary_storage")

# Base class
BinaryStorage = binary_storage_ns.class_("BinaryStorage", cg.Component)

# Configuration schema will be extended by specific device types (FRAM, EEPROM, etc.)
# This is a placeholder for the base component
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BinaryStorage),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Base to_code - specific devices will override this."""
    # This will be overridden by specific device implementations
    pass
