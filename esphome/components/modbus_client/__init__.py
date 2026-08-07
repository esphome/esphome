from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ON_ERROR, CONF_ON_RESPONSE
from esphome.core import Lambda
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@exciton"]
DEPENDENCIES = ["modbus"]

CONF_ON_NO_RESPONSE = "on_no_response"
CONF_ON_NOT_SENT = "on_not_sent"
CONF_ON_SENT = "on_sent"
CONF_PDU = "pdu"
CONF_RETRY = "retry"

modbus_client_ns = cg.esphome_ns.namespace("modbus_client")
ModbusClientSendAction = modbus_client_ns.class_(
    "ModbusClientSendAction", automation.Action, modbus.ModbusClientDevice
)

# The exception code passed to on_error handlers.
ExceptionCode = modbus.modbus_ns.enum("ExceptionCode")

# Lambda argument types for the reply handlers: the device address the send targeted, and the
# request/response PDUs (function code + data). The spans are only valid for the duration of the handler.
_PDU_SPAN = cg.std_span.template(cg.uint8.operator("const"))

# The pdu lambda's return type: a stack-allocated StaticVector capped at the Modbus PDU limit
# (modbus.MAX_PDU_SIZE). Lambdas can return a byte list or a modbus::helpers::create_*_pdu() result.
# The list form below is bounded by cv.Length; a lambda cannot be. PduBuffer drops bytes past
# modbus.MAX_PDU_SIZE without reporting it, so an over-long lambda PDU is silently truncated.
_PDU_BUFFER = modbus.modbus_ns.namespace("helpers").class_("PduBuffer")


def _synchronous_handler(value: ConfigType) -> ConfigType:
    """Reject deferring actions in a handler: its PDU spans point into hub buffers that are reused
    once the handler returns, and DelayAction and friends capture the trigger args for later replay."""
    if automation.has_non_synchronous_actions(value):
        raise cv.Invalid(
            "Deferring actions (delay, wait_until, script.wait, ...) are not allowed in modbus_client "
            "handlers: the request/response data is only valid while the handler runs. Copy what you "
            "need into globals first, then defer in a separate script or automation."
        )
    return value


def _handler_schema() -> cv.All:
    return cv.All(automation.validate_automation(single=True), _synchronous_handler)


# Each action is its own hub device: the modbus hub routes the reply straight back to the action that
# sent it, so the address can even be templatable - the reply is matched by the action's identity, not
# its address.
_ACTION_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(modbus.CONF_MODBUS_ID): cv.use_id(modbus.ModbusClient),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.hex_uint8_t),
        # Optional handlers. on_sent fires when the frame reaches the wire; the reply handlers arrive
        # later (fire-and-continue), so all run with the request/reply available - not the outer
        # automation's variables.
        cv.Optional(CONF_ON_SENT): _handler_schema(),
        cv.Optional(CONF_ON_ERROR): _handler_schema(),
        # on_no_response takes either a returning lambda (`!lambda "return <bool>;"`, gets `request`,
        # returns true to have the hub retry the frame) OR a `then:` automation of actions; the automation
        # form may also carry an optional `retry:` returning lambda to run actions AND decide the retry.
        cv.Optional(CONF_ON_NO_RESPONSE): cv.All(
            cv.Any(
                cv.returning_lambda,
                automation.validate_automation(
                    {cv.Optional(CONF_RETRY): cv.returning_lambda}, single=True
                ),
            ),
            _synchronous_handler,
        ),
        cv.Optional(CONF_ON_NOT_SENT): _handler_schema(),
    }
)

MODBUS_CLIENT_SEND_SCHEMA = _ACTION_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_PDU): cv.templatable(
            cv.All(
                cv.ensure_list(cv.hex_uint8_t),
                cv.Length(min=1, max=modbus.MAX_PDU_SIZE),
            )
        ),
        cv.Optional(CONF_ON_RESPONSE): _handler_schema(),
    }
)


async def register_client_action(
    var: cg.MockObj,
    config: ConfigType,
    args: TemplateArgsType,
    response_args: TemplateArgsType,
) -> cg.MockObj:
    """Wire the shared action plumbing: hub parent, templated device address, outcome triggers.

    response_args are the on_response handler's arguments, which differ per action.
    """
    parent = await cg.get_variable(config[modbus.CONF_MODBUS_ID])
    cg.add(var.set_parent(parent))
    cg.add(
        var.set_target_address(
            await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)
        )
    )
    if sent_conf := config.get(CONF_ON_SENT):
        await automation.build_automation(
            var.get_sent_trigger(), [(_PDU_SPAN, "request")], sent_conf
        )
    if response_conf := config.get(CONF_ON_RESPONSE):
        await automation.build_automation(
            var.get_response_trigger(), response_args, response_conf
        )
    if error_conf := config.get(CONF_ON_ERROR):
        await automation.build_automation(
            var.get_error_trigger(),
            [(_PDU_SPAN, "request"), (ExceptionCode, "exception_code")],
            error_conf,
        )
    if (no_response_conf := config.get(CONF_ON_NO_RESPONSE)) is not None:
        # The lambda form IS the retry decision; the automation form runs actions and may carry a nested
        # `retry:` lambda. Either way the retry lambda's bool becomes on_no_response()'s return value.
        if isinstance(no_response_conf, Lambda):
            retry_conf = no_response_conf
        else:
            await automation.build_automation(
                var.get_no_response_trigger(),
                [(_PDU_SPAN, "request")],
                no_response_conf,
            )
            retry_conf = no_response_conf.get(CONF_RETRY)
        if retry_conf is not None:
            retry_lambda = await cg.process_lambda(
                retry_conf, [(_PDU_SPAN, "request")], return_type=cg.bool_
            )
            cg.add(var.set_retry(retry_lambda))
    if not_sent_conf := config.get(CONF_ON_NOT_SENT):
        await automation.build_automation(
            var.get_not_sent_trigger(), [(_PDU_SPAN, "request")], not_sent_conf
        )
    return var


@automation.register_action(
    "modbus_client.send",
    ModbusClientSendAction,
    MODBUS_CLIENT_SEND_SCHEMA,
    synchronous=True,
)
async def modbus_client_send_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    template_ = await cg.templatable(config[CONF_PDU], args, _PDU_BUFFER)
    cg.add(var.set_pdu(template_))
    return await register_client_action(
        var,
        config,
        args,
        [(_PDU_SPAN, "request"), (_PDU_SPAN, "response")],
    )
