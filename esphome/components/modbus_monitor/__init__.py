from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
from esphome.components.const import CONF_ON_REQUEST
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["modbus"]
MULTI_CONF = True

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(modbus.CONF_MODBUS_ID): cv.use_id(modbus.Modbus),
        cv.Required(CONF_ON_REQUEST): cv.All(
            automation.validate_automation(single=True),
            modbus.synchronous_handler("modbus_monitor"),
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[modbus.CONF_MODBUS_ID])
    await modbus.register_on_request_automation(hub, config[CONF_ON_REQUEST])
