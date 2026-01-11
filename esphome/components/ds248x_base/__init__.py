"""Base class for DS248x I2C-to-1-Wire bridge chips."""

import esphome.codegen as cg
from esphome.components import i2c, one_wire

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["i2c", "one_wire"]

ds248x_base_ns = cg.esphome_ns.namespace("ds248x_base")
DS248xOneWireBusBase = ds248x_base_ns.class_(
    "DS248xOneWireBusBase", one_wire.OneWireBus, i2c.I2CDevice, cg.Component
)
