from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ON_RESPONSE

DEPENDENCIES = ["modbus"]
MULTI_CONF = True

# A span, not a copy, matching modbus_client.
_PAYLOAD_SPAN = cg.std_span.template(cg.uint8.operator("const"))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(modbus.CONF_MODBUS_ID): cv.use_id(modbus.ModbusSniffer),
        cv.Optional(CONF_ON_RESPONSE): automation.validate_automation(single=True),
    }
)


async def to_code(config):
    # The hub IS the sniffer: `role: sniffer` creates it and owns the uart. This only attaches
    # behaviour to it.
    hub = await cg.get_variable(config[modbus.CONF_MODBUS_ID])

    if on_response := config.get(CONF_ON_RESPONSE):
        await automation.build_automation(
            hub.get_response_trigger(),
            [
                (cg.uint8, "address"),
                (cg.uint8, "function_code"),
                (cg.uint16, "start_address"),
                (_PAYLOAD_SPAN, "payload"),
            ],
            on_response,
        )
