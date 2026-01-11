"""DS2482-800 8-channel I2C-to-1-Wire bridge."""

from dataclasses import dataclass, field

import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.one_wire import OneWireBus
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.core import CORE

DOMAIN = "ds2482"

ds2482_ns = cg.esphome_ns.namespace("ds2482")

CONF_ACTIVE_PULLUP = "active_pullup"
CONF_STRONG_PULLUP = "strong_pullup"
CONF_CHANNEL = "channel"

CODEOWNERS = ["@bytenik"]
AUTO_LOAD = ["ds248x_base"]
DEPENDENCIES = ["i2c"]

DS2482OneWireBus = ds2482_ns.class_(
    "DS2482OneWireBus", OneWireBus, i2c.I2CDevice, cg.Component
)


@dataclass
class DS2482ChipConfig:
    """Tracks configuration for a DS2482 chip (by I2C address)."""

    address: int
    active_pullup: bool
    strong_pullup: bool
    channels: list[int] = field(default_factory=list)


def _get_data() -> dict[int, DS2482ChipConfig]:
    """Get DS2482 chip tracking data from CORE.data."""
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = {}
    return CORE.data[DOMAIN]


def validate_config(config):
    """Validate that all channels on the same chip have consistent pullup config."""
    data = _get_data()
    address = config.get(CONF_ADDRESS, 0x18)
    channel = config[CONF_CHANNEL]
    active_pullup = config[CONF_ACTIVE_PULLUP]
    strong_pullup = config[CONF_STRONG_PULLUP]

    if address in data:
        # Chip already configured by another channel
        existing = data[address]
        if existing.active_pullup != active_pullup:
            raise cv.Invalid(
                f"Channel {channel} has active_pullup={active_pullup} but "
                f"channel(s) {existing.channels} on same chip (address 0x{address:02X}) "
                f"have active_pullup={existing.active_pullup}. "
                f"All channels on the same DS2482 chip must have identical pullup settings."
            )
        if existing.strong_pullup != strong_pullup:
            raise cv.Invalid(
                f"Channel {channel} has strong_pullup={strong_pullup} but "
                f"channel(s) {existing.channels} on same chip (address 0x{address:02X}) "
                f"have strong_pullup={existing.strong_pullup}. "
                f"All channels on the same DS2482 chip must have identical pullup settings."
            )
        existing.channels.append(channel)
    else:
        # First channel for this chip
        data[address] = DS2482ChipConfig(
            address=address,
            active_pullup=active_pullup,
            strong_pullup=strong_pullup,
            channels=[channel],
        )

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS2482OneWireBus),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
            cv.Optional(CONF_ACTIVE_PULLUP, default=False): cv.boolean,
            cv.Optional(CONF_STRONG_PULLUP, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18)),
    validate_config,  # Cross-instance validation
)


async def to_code(config):
    """Generate code for DS2482 component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)
    await cg.register_component(var, config)
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_active_pullup(config[CONF_ACTIVE_PULLUP]))
    cg.add(var.set_strong_pullup(config[CONF_STRONG_PULLUP]))
