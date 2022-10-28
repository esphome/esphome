import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)
from esphome.components import uart

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@glmnet"]
MULTI_CONF = True

sim800l_ns = cg.esphome_ns.namespace("sim800l")
Sim800LComponent = sim800l_ns.class_("Sim800LComponent", cg.Component)

Sim800LReceivedMessageTrigger = sim800l_ns.class_(
    "Sim800LReceivedMessageTrigger",
    automation.Trigger.template(cg.std_string, cg.std_string),
)
Sim800LIncomingCallTrigger = sim800l_ns.class_(
    "Sim800LIncomingCallTrigger",
    automation.Trigger.template(cg.std_string),
)
Sim800LCallConnectedTrigger = sim800l_ns.class_(
    "Sim800LCallConnectedTrigger",
    automation.Trigger.template(),
)
Sim800LCallDisconnectedTrigger = sim800l_ns.class_(
    "Sim800LCallDisconnectedTrigger",
    automation.Trigger.template(),
)

Sim800LReceivedUssdTrigger = sim800l_ns.class_(
    "Sim800LReceivedUssdTrigger",
    automation.Trigger.template(cg.std_string),
)

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
CONF_MESSAGE = "message"
CONF_USSD = "ussd"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Sim800LComponent),
            cv.Optional(CONF_ON_SMS_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim800LReceivedMessageTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_INCOMING_CALL): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim800LIncomingCallTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_CALL_CONNECTED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim800LCallConnectedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_CALL_DISCONNECTED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim800LCallDisconnectedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_USSD_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim800LReceivedUssdTrigger
                    ),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "sim800l", require_tx=True, require_rx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    for conf in config.get(CONF_ON_SMS_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "message"), (cg.std_string, "sender")], conf
        )
    for conf in config.get(CONF_ON_INCOMING_CALL, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "caller_id")], conf)
    for conf in config.get(CONF_ON_CALL_CONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CALL_DISCONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_USSD_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "ussd")], conf)


SIM800L_SEND_SMS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sim800LComponent),
        cv.Required(CONF_RECIPIENT): cv.templatable(cv.string_strict),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "sim800l.send_sms", Sim800LSendSmsAction, SIM800L_SEND_SMS_SCHEMA
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


@automation.register_action("sim800l.dial", Sim800LDialAction, SIM800L_DIAL_SCHEMA)
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
)
async def sim800l_connect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


SIM800L_SEND_USSD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sim800LComponent),
        cv.Required(CONF_USSD): cv.templatable(cv.string_strict),
    }
)


@automation.register_action(
    "sim800l.send_ussd", Sim800LSendUssdAction, SIM800L_SEND_USSD_SCHEMA
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
)
async def sim800l_disconnect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var
