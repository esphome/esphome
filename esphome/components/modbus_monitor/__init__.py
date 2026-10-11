from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ON_RESPONSE
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["modbus"]
DOMAIN = "modbus_monitor"
MULTI_CONF = True

CONF_ON_REQUEST = "on_request"


def _handler() -> cv.All:
    return cv.All(
        automation.validate_automation(single=True),
        modbus.synchronous_handler("modbus_monitor"),
    )


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(modbus.CONF_MODBUS_ID): cv.use_id(modbus.Modbus),
            cv.Optional(CONF_ON_REQUEST): _handler(),
            cv.Optional(CONF_ON_RESPONSE): _handler(),
        }
    ),
    cv.has_at_least_one_key(CONF_ON_REQUEST, CONF_ON_RESPONSE),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[modbus.CONF_MODBUS_ID])
    if on_request := config.get(CONF_ON_REQUEST):
        await modbus.register_on_request_automation(hub, on_request)
    if on_response := config.get(CONF_ON_RESPONSE):
        await modbus.register_on_response_automation(hub, on_response)
