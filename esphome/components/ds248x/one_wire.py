"""DS248x 1-Wire Bus Platform.

This platform creates one_wire bus instances backed by a DS248x I2C-to-1-Wire bridge.
It supports DS2482-100/101 (single channel), DS2482-800 (8 channels), and DS2484 (single channel).

For multi-channel devices (DS2482-800), create one platform entry per channel.
Each entry becomes a separate one_wire bus that can be used by dallas_temp and other 1-Wire devices.
"""

from esphome import final_validate as fv
import esphome.codegen as cg
from esphome.components.one_wire import OneWireBus
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from . import CONF_DS248X_ID, DS248xComponent, ds248x_ns, get_channel_count

CODEOWNERS = ["@tomwellnitz"]
DEPENDENCIES = ["ds248x"]

DS248xOneWireBus = ds248x_ns.class_("DS248xOneWireBus", OneWireBus, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DS248xOneWireBus),
        cv.GenerateID(CONF_DS248X_ID): cv.use_id(DS248xComponent),
        cv.Optional(CONF_CHANNEL, default=0): cv.int_range(min=0, max=7),
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config: ConfigType) -> None:
    """Validate that the channel is within the parent's channel count."""
    fconf = fv.full_config.get()
    path = fconf.get_path_for_id(config[CONF_DS248X_ID])[:-1]
    parent_config = fconf.get_config_for_path(path)
    channel_count = get_channel_count(parent_config)
    channel = config[CONF_CHANNEL]

    if channel >= channel_count:
        raise cv.Invalid(
            f"Channel {channel} is invalid for DS248x with {channel_count} channel(s). "
            f"Valid range: 0-{channel_count - 1}"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_DS248X_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
