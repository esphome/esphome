import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@zweckj"]
DEPENDENCIES = ["modbus"]
MULTI_CONF = True

CONF_HOERMANN_HCP_ID = "hoermann_hcp_id"

hoermann_hcp_ns = cg.esphome_ns.namespace("hoermann_hcp")
HoermannHcp = hoermann_hcp_ns.class_(
    "HoermannHcp", cg.PollingComponent, modbus.ModbusServerDevice
)

# The Hoermann UAP module answers on Modbus server address 2.
CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(HoermannHcp)})
    .extend(cv.polling_component_schema("500ms"))
    .extend(modbus.modbus_device_schema(0x02, role="server"))
)

FINAL_VALIDATE_SCHEMA = modbus.final_validate_modbus_device(
    "hoermann_hcp", role="server"
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_server_device(var, config)
