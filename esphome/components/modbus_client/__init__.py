from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ON_ERROR, CONF_ON_RESPONSE

CODEOWNERS = ["@exciton"]
DEPENDENCIES = ["modbus"]

CONF_ON_NO_RESPONSE = "on_no_response"
CONF_ON_NOT_SENT = "on_not_sent"
CONF_PDU = "pdu"

modbus_client_ns = cg.esphome_ns.namespace("modbus_client")
ModbusClientSendAction = modbus_client_ns.class_(
    "ModbusClientSendAction", automation.Action, modbus.ModbusClientDevice
)

# The exception code passed to on_error handlers.
ModbusExceptionCode = modbus.modbus_ns.enum("ModbusExceptionCode")

# Lambda argument types for the reply handlers: the device address the send targeted, and the
# request/response PDUs (function code + data). The spans are only valid for the duration of the handler.
_PDU_SPAN = cg.std_span.template(cg.uint8.operator("const"))

# Each action is its own hub device: the modbus hub routes the reply straight back to the action that
# sent it, so the address can even be templatable - the reply is matched by the action's identity, not
# its address.
_ACTION_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(modbus.CONF_MODBUS_ID): cv.use_id(modbus.ModbusClient),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.hex_uint8_t),
        # Optional reply handlers. The reply arrives later (fire-and-continue), so these run when it
        # does, with the address and reply available - not the outer automation's variables.
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_NO_RESPONSE): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_NOT_SENT): automation.validate_automation(single=True),
    }
)

MODBUS_CLIENT_SEND_SCHEMA = _ACTION_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_PDU): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
        cv.Optional(CONF_ON_RESPONSE): automation.validate_automation(single=True),
    }
)


async def register_client_action(var, config, args, response_args):
    """Wire the shared action plumbing: hub parent, templated device address, outcome triggers."""
    parent = await cg.get_variable(config[modbus.CONF_MODBUS_ID])
    cg.add(var.set_parent(parent))
    cg.add(
        var.set_target_address(
            await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)
        )
    )
    if response_conf := config.get(CONF_ON_RESPONSE):
        await automation.build_automation(
            var.get_response_trigger(), response_args, response_conf
        )
    if error_conf := config.get(CONF_ON_ERROR):
        await automation.build_automation(
            var.get_error_trigger(),
            [(_PDU_SPAN, "request"), (ModbusExceptionCode, "exception_code")],
            error_conf,
        )
    if no_response_conf := config.get(CONF_ON_NO_RESPONSE):
        await automation.build_automation(
            var.get_no_response_trigger(), [], no_response_conf
        )
    if not_sent_conf := config.get(CONF_ON_NOT_SENT):
        await automation.build_automation(var.get_not_sent_trigger(), [], not_sent_conf)
    return var


@automation.register_action(
    "modbus_client.send",
    ModbusClientSendAction,
    MODBUS_CLIENT_SEND_SCHEMA,
    synchronous=True,
)
async def modbus_client_send_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    template_ = await cg.templatable(
        config[CONF_PDU], args, cg.std_vector.template(cg.uint8)
    )
    cg.add(var.set_pdu(template_))
    return await register_client_action(
        var,
        config,
        args,
        [(_PDU_SPAN, "request"), (_PDU_SPAN, "response")],
    )
