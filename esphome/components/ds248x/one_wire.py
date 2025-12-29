"""DS248x 1-Wire Bus Platform.

This platform creates one_wire bus instances backed by a DS248x I2C-to-1-Wire bridge.
It supports DS2482-100 (single channel), DS2482-800 (8 channels), and DS2484 (single channel).

For multi-channel devices (DS2482-800), create one platform entry per channel.
Each entry becomes a separate one_wire bus that can be used by dallas_temp and other 1-Wire devices.
"""

import esphome.codegen as cg
from esphome.components.one_wire import OneWireBus
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from . import CONF_DS248X_ID, DS248xComponent, ds248x_ns

CODEOWNERS = ["@tomwellnitz"]

DS248xOneWireBus = ds248x_ns.class_(
    "DS248xOneWireBus", OneWireBus, cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DS248xOneWireBus),
        cv.GenerateID(CONF_DS248X_ID): cv.use_id(DS248xComponent),
        cv.Optional(CONF_CHANNEL, default=0): cv.int_range(min=0, max=7),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_channel(config):
    """Validate that the channel is valid for the parent's channel count."""
    # This validation happens at runtime since we don't have access to parent config here
    return config


CONFIG_SCHEMA = CONFIG_SCHEMA.add_extra(_validate_channel)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_DS248X_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))

    # Register this bus with the parent for lifecycle management
    cg.add(parent.register_bus(var))
