import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@zweckj"]
DEPENDENCIES = ["modbus"]

CONF_HOERMANN_ID = "hoermann_id"

hoermann_ns = cg.esphome_ns.namespace("hoermann")
Hoermann = hoermann_ns.class_(
    "Hoermann", cg.PollingComponent, modbus.ModbusServerDevice
)

# The Hoermann UAP module answers on Modbus server address 2.
CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(Hoermann)})
    .extend(cv.polling_component_schema("500ms"))
    .extend(modbus.modbus_device_schema(0x02, role="server"))
)

FINAL_VALIDATE_SCHEMA = modbus.final_validate_modbus_device("hoermann", role="server")


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_server_device(var, config)
