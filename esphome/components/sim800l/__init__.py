from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MESSAGE

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@glmnet"]
MULTI_CONF = True

sim800l_ns = cg.esphome_ns.namespace("sim800l")
Sim800LComponent = sim800l_ns.class_("Sim800LComponent", cg.Component)

# Actions
Sim800LSendSmsAction = sim800l_ns.class_("Sim800LSendSmsAction", automation.Action)
Sim800LSendUssdAction = sim800l_ns.class_("Sim800LSendUssdAction", automation.Action)
Sim800LDialAction = sim800l_ns.class_("Sim800LDialAction", automation.Action)
Sim800LConnectAction = sim800l_ns.class_("Sim800LConnectAction", automation.Action)
Sim800LDisconnectAction = sim800l_ns.class_(
    "Sim800LDisconnectAction", automation.Action
)

CONF_SIM800L_ID = "sim800l_id"
CONF_ON_SMS_RECEIVED = "on_sms_received"
CONF_ON_USSD_RECEIVED = "on_ussd_received"
CONF_ON_INCOMING_CALL = "on_incoming_call"
CONF_ON_CALL_CONNECTED = "on_call_connected"
CONF_ON_CALL_DISCONNECTED = "on_call_disconnected"
CONF_RECIPIENT = "recipient"
CONF_USSD = "ussd"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Sim800LComponent),
            cv.Optional(CONF_ON_SMS_RECEIVED): automation.validate_automation({}),
            cv.Optional(CONF_ON_INCOMING_CALL): automation.validate_automation({}),
            cv.Optional(CONF_ON_CALL_CONNECTED): automation.validate_automation({}),
            cv.Optional(CONF_ON_CALL_DISCONNECTED): automation.validate_automation({}),
            cv.Optional(CONF_ON_USSD_RECEIVED): automation.validate_automation({}),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "sim800l", require_tx=True, require_rx=True
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_SMS_RECEIVED,
        "add_on_sms_received_callback",
        [(cg.std_string, "message"), (cg.std_string, "sender")],
    ),
    automation.CallbackAutomation(
        CONF_ON_INCOMING_CALL,
        "add_on_incoming_call_callback",
        [(cg.std_string, "caller_id")],
    ),
    automation.CallbackAutomation(
        CONF_ON_CALL_CONNECTED, "add_on_call_connected_callback"
    ),
    automation.CallbackAutomation(
        CONF_ON_CALL_DISCONNECTED, "add_on_call_disconnected_callback"
    ),
    automation.CallbackAutomation(
        CONF_ON_USSD_RECEIVED,
        "add_on_ussd_received_callback",
        [(cg.std_string, "ussd")],
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


SIM800L_SEND_SMS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sim800LComponent),
        cv.Required(CONF_RECIPIENT): cv.templatable(cv.string_strict),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "sim800l.send_sms",
    Sim800LSendSmsAction,
    SIM800L_SEND_SMS_SCHEMA,
    synchronous=True,
)
async def sim800l_send_sms_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_RECIPIENT], args, cg.std_string)
    cg.add(var.set_recipient(template_))
    template_ = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(template_))
    return var


SIM800L_DIAL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sim800LComponent),
        cv.Required(CONF_RECIPIENT): cv.templatable(cv.string_strict),
    }
)


@automation.register_action(
    "sim800l.dial", Sim800LDialAction, SIM800L_DIAL_SCHEMA, synchronous=True
)
async def sim800l_dial_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_RECIPIENT], args, cg.std_string)
    cg.add(var.set_recipient(template_))
    return var


@automation.register_action(
    "sim800l.connect",
    Sim800LConnectAction,
    cv.Schema({cv.GenerateID(): cv.use_id(Sim800LComponent)}),
    synchronous=True,
)
async def sim800l_connect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


SIM800L_SEND_USSD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sim800LComponent),
        cv.Required(CONF_USSD): cv.templatable(cv.string_strict),
    }
)


@automation.register_action(
    "sim800l.send_ussd",
    Sim800LSendUssdAction,
    SIM800L_SEND_USSD_SCHEMA,
    synchronous=True,
)
async def sim800l_send_ussd_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_USSD], args, cg.std_string)
    cg.add(var.set_ussd(template_))
    return var


@automation.register_action(
    "sim800l.disconnect",
    Sim800LDisconnectAction,
    cv.Schema({cv.GenerateID(): cv.use_id(Sim800LComponent)}),
    synchronous=True,
)
async def sim800l_disconnect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
