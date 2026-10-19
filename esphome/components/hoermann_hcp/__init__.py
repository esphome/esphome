import logging

import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@zweckj"]
DEPENDENCIES = ["modbus"]
DOMAIN = "hoermann_hcp"
MULTI_CONF = True

CONF_HOERMANN_HCP_ID = "hoermann_hcp_id"

hoermann_hcp_ns = cg.esphome_ns.namespace("hoermann_hcp")
HoermannHcp = hoermann_hcp_ns.class_(
    "HoermannHcp", cg.PollingComponent, modbus.ModbusServerDevice
)

# Default Modbus server address: 0x2A (42), a free accessory slot in the middle
# of the unit ID range the drive sweeps during its bus scan.
HCP2_DEFAULT_ADDRESS = 0x2A

# Addresses that must not be used by a custom accessory, because they are
# fixed to official accessories on the HCP2 bus:
#   0x01 = HAP 1 HCP
#   0x02 = UAP 1 HCP
#   0x0A = Hörmann KNX Gateway
#   0x0F = Loxone Hörmann Air
#
# Update in 2027.04.0
# 0x02 (UAP 1 HCP) is intentionally not in this tuple during the deprecation
# window. See _validate_deprecated_hcp2_address below.
RESERVED_HCP2_ADDRESSES = (0x01, 0x0A, 0x0F)

# The drive only sweeps unit ids 0x01..0xF3 during its bus scan, so anything
# above 0xF3 can never be reached.
HCP2_MAX_ADDRESS = 0xF3


def _validate_hcp2_address(value: int) -> int:
    if value in RESERVED_HCP2_ADDRESSES:
        raise cv.Invalid(
            f"Modbus address 0x{value:02X} is fixed to an official HCP2 "
            "accessory on the HCP2 bus; please pick a different address"
        )
    return value


# Remove in 2027.04.0
#
# The previous hoermann_hcp default was 0x02, which collides with the Hörmann
# UAP 1 HCP accessory on the HCP2 bus. During the deprecation window the
# default stays 0x02 (with a warning). In 2027.04.0 our address changes to 0x2A
# and 0x02 will be hard-rejected.
_DEPRECATED_HCP2_ADDRESS = 0x02


# Remove in 2027.04.0
def _validate_deprecated_hcp2_address(value: int) -> int:
    """Warn (but accept) the old default 0x02 during the deprecation window."""
    if value == _DEPRECATED_HCP2_ADDRESS:
        _LOGGER.warning(
            "[hoermann_hcp] Using 'address: 0x02' is being deprecated because "
            "it is a reserved HCP2 address. In 2027.04.0 the default address "
            "will change to 0x2A and 0x02 will be rejected. "
            "Set 'address: 0x2A' and run a bus scan now to future-proof your setup."
        )
    return value


# Remove in 2027.04.0
def _warn_missing_hcp2_address(config: ConfigType) -> ConfigType:
    """Keep the old default 0x02 (and warn) when 'address' is not set."""
    if CONF_ADDRESS not in config:
        _LOGGER.warning(
            "[hoermann_hcp] 'address' not set. Using 0x02 for now. Default HCP2 address "
            "will change to 0x2A in 2027.04.0. Set 'address: 0x2A' explicitly to silence "
            "this warning."
        )
        config[CONF_ADDRESS] = _DEPRECATED_HCP2_ADDRESS
    return config


CONFIG_SCHEMA = cv.All(
    (
        cv.Schema({cv.GenerateID(): cv.declare_id(HoermannHcp)})
        .extend(cv.polling_component_schema("500ms"))
        .extend(modbus.modbus_device_schema(None, role="server"))
        .extend(
            {
                cv.Optional(CONF_ADDRESS): cv.All(
                    cv.hex_uint8_t,
                    cv.int_range(min=1, max=HCP2_MAX_ADDRESS),
                    _validate_deprecated_hcp2_address,  # Remove in 2027.04.0
                    _validate_hcp2_address,
                )
            }
        )
    ),
    _warn_missing_hcp2_address,  # Remove in 2027.04.0
)

FINAL_VALIDATE_SCHEMA = modbus.final_validate_modbus_device(
    "hoermann_hcp", role="server"
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_server_device(var, config)
