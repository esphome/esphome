from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
from esphome.components.const import CONF_ON_REQUEST, CONF_ROLE
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["modbus"]
MULTI_CONF = True

# The hub roles a monitor can watch: a server reads requests, a client sends them.
ROLES = ("client", "server")

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(modbus.CONF_MODBUS_ID): cv.use_id(modbus.Modbus),
            cv.Optional(CONF_ON_REQUEST): cv.All(
                automation.validate_automation(single=True),
                modbus.synchronous_handler("modbus_monitor"),
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_ON_REQUEST),
)


def validate_role(role: str) -> str:
    if role not in ROLES:
        raise cv.Invalid(
            f"modbus_monitor needs a hub with role client or server, not '{role}'"
        )
    return role


FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Required(modbus.CONF_MODBUS_ID): fv.id_declaration_match_schema(
            {cv.Required(CONF_ROLE): validate_role}
        ),
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[modbus.CONF_MODBUS_ID])
    await modbus.register_on_request_automation(hub, config[CONF_ON_REQUEST])
