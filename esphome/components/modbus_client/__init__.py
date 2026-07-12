from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_ERROR, CONF_ON_RESPONSE

CODEOWNERS = ["@exciton"]
DEPENDENCIES = ["modbus"]
MULTI_CONF = True

CONF_ON_NO_RESPONSE = "on_no_response"
CONF_ON_NOT_SENT = "on_not_sent"
CONF_PDU = "pdu"

modbus_client_ns = cg.esphome_ns.namespace("modbus_client")
ModbusCallbackClient = modbus_client_ns.class_(
    "ModbusCallbackClient", modbus.ModbusClientDevice, cg.Component
)
ModbusClientSendAction = modbus_client_ns.class_(
    "ModbusClientSendAction", automation.Action
)

# The exception code passed to on_error handlers.
ModbusExceptionCode = modbus.modbus_ns.enum("ModbusExceptionCode")

# Lambda argument types shared by the callbacks: the request/response PDUs (function code + data). The
# spans are only valid for the duration of the callback.
_PDU_SPAN = cg.std_span.template(cg.uint8.operator("const"))

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusCallbackClient),
            cv.Optional(CONF_ON_RESPONSE): automation.validate_automation({}),
            cv.Optional(CONF_ON_ERROR): automation.validate_automation({}),
            cv.Optional(CONF_ON_NO_RESPONSE): automation.validate_automation({}),
            cv.Optional(CONF_ON_NOT_SENT): automation.validate_automation({}),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(modbus.modbus_device_schema(None))
)

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_RESPONSE,
        "add_on_response",
        [(_PDU_SPAN, "request"), (_PDU_SPAN, "response")],
    ),
    automation.CallbackAutomation(
        CONF_ON_ERROR,
        "add_on_error",
        [
            (_PDU_SPAN, "request"),
            (ModbusExceptionCode, "exception_code"),
        ],
    ),
    automation.CallbackAutomation(
        CONF_ON_NO_RESPONSE,
        "add_on_no_response",
        [(_PDU_SPAN, "request")],
    ),
    automation.CallbackAutomation(
        CONF_ON_NOT_SENT,
        "add_on_not_sent",
        [(_PDU_SPAN, "request")],
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_client_device(var, config)
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


MODBUS_CLIENT_SEND_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ModbusCallbackClient),
        cv.Required(CONF_PDU): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
        # Optional per-send reply handlers. The reply arrives later (fire-and-continue), so these run when it
        # does, with only the reply available - not the outer automation's variables.
        cv.Optional(CONF_ON_RESPONSE): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_NO_RESPONSE): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_NOT_SENT): automation.validate_automation(single=True),
    }
)


@automation.register_action(
    "modbus_client.send",
    ModbusClientSendAction,
    MODBUS_CLIENT_SEND_SCHEMA,
    synchronous=True,
)
async def modbus_client_send_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(
        config[CONF_PDU], args, cg.std_vector.template(cg.uint8)
    )
    cg.add(var.set_pdu(template_))

    has_handlers = False
    if response_conf := config.get(CONF_ON_RESPONSE):
        has_handlers = True
        await automation.build_automation(
            var.get_response_trigger(),
            [(_PDU_SPAN, "request"), (_PDU_SPAN, "response")],
            response_conf,
        )
    if error_conf := config.get(CONF_ON_ERROR):
        has_handlers = True
        await automation.build_automation(
            var.get_error_trigger(),
            [(_PDU_SPAN, "request"), (ModbusExceptionCode, "exception_code")],
            error_conf,
        )
    if no_response_conf := config.get(CONF_ON_NO_RESPONSE):
        has_handlers = True
        await automation.build_automation(
            var.get_no_response_trigger(), [], no_response_conf
        )
    if not_sent_conf := config.get(CONF_ON_NOT_SENT):
        has_handlers = True
        await automation.build_automation(var.get_not_sent_trigger(), [], not_sent_conf)
    if has_handlers:
        cg.add(var.set_has_response_handlers(True))
    return var
