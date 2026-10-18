import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.types import ConfigType

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
RESERVED_HOERMANN_ADDRESSES = (0x01, 0x02, 0x0A, 0x0F)

# The drive only sweeps unit ids 0x01..0xF3 during its bus scan, so anything
# above 0xF3 can never be reached.
HCP2_MAX_ADDRESS = 0xF3


def _validate_hcp2_address(value: int) -> int:
    if value in RESERVED_HOERMANN_ADDRESSES:
        raise cv.Invalid(
            f"Modbus address 0x{value:02X} is fixed to an official Hörmann "
            "accessory on the HCP2 bus; please pick a different address"
        )
    return value


CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(HoermannHcp)})
    .extend(cv.polling_component_schema("500ms"))
    .extend(modbus.modbus_device_schema(HCP2_DEFAULT_ADDRESS, role="server"))
    # Re-declare CONF_ADDRESS with Hörmann-specific validation. The plain
    # # modbus_device_schema only checks for a sane Modbus server address.
    .extend(
        {
            cv.Optional(
                CONF_ADDRESS, default=HCP2_DEFAULT_ADDRESS
            ): cv.All(
                cv.hex_uint8_t,
                cv.int_range(min=1, max=HCP2_MAX_ADDRESS),
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
