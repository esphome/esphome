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


# Scheduled for removal in 2027.06.0.
#
# Remove _DEPRECATED_HCP2_ADDRESS, _validate_deprecated_hcp2_address and the
# _validate_deprecated_hcp2_address call in CONFIG_SCHEMA below, then
# add 0x02 back to RESERVED_HCP2_ADDRESSES so the address is hard-rejected
# like the other official accessory addresses.
#
# The previous hoermann_hcp default was 0x02, which collides with the Hörmann's
# UAP 1 HCP accessory. Existing configs that pin `address: 0x02` in YAML still build
# during the deprecation window but will throw a warning.
_DEPRECATED_HCP2_ADDRESS = 0x02


def _validate_deprecated_hcp2_address(value: int) -> int:
    """Warn (but accept) the old default 0x02 during the deprecation window."""
    if value == _DEPRECATED_HCP2_ADDRESS:
        _LOGGER.warning(
            "[hoermann_hcp] 'address: 0x02' is deprecated! It collides with the "
            "official UAP 1 HCP accessory on the HCP2 bus. Default address has changed to "
            "0x2A. Remove 'address: 0x02' and run a bus scan on the drive. 'address: "
            "0x02' will be rejected in 2027.06.0."
        )
    return value


CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(HoermannHcp)})
    .extend(cv.polling_component_schema("500ms"))
    # Pass default_address=None so the modbus helper declares CONF_ADDRESS
    # without a default. Hörmann-specific re-declaration below is the single
    # source of the default. The helper's server-address validator is replaced
    # by this re-declaration, but cv.int_range(min=1, ...) still rejects the
    # broadcast address (0).
    .extend(modbus.modbus_device_schema(None, role="server"))
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=HCP2_DEFAULT_ADDRESS): cv.All(
                cv.hex_uint8_t,
                cv.int_range(min=1, max=HCP2_MAX_ADDRESS),
                _validate_deprecated_hcp2_address,
                _validate_hcp2_address,
            )
        }
    )
)

FINAL_VALIDATE_SCHEMA = modbus.final_validate_modbus_device(
    "hoermann_hcp", role="server"
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_server_device(var, config)
