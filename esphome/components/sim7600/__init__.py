import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)
from esphome.components import uart

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@chino-lu"]
MULTI_CONF = True

sim7600_ns = cg.esphome_ns.namespace("sim7600")
Sim7600Component = sim7600_ns.class_("Sim7600Component", cg.Component)

Sim7600ReceivedMessageTrigger = sim7600_ns.class_(
    "Sim7600ReceivedMessageTrigger",
    automation.Trigger.template(cg.std_string, cg.std_string),
)
Sim7600IncomingCallTrigger = sim7600_ns.class_(
    "Sim7600IncomingCallTrigger",
    automation.Trigger.template(cg.std_string),
)
Sim7600CallConnectedTrigger = sim7600_ns.class_(
    "Sim7600CallConnectedTrigger",
    automation.Trigger.template(),
)
Sim7600CallDisconnectedTrigger = sim7600_ns.class_(
    "Sim7600CallDisconnectedTrigger",
    automation.Trigger.template(),
)

Sim7600ReceivedUssdTrigger = sim7600_ns.class_(
    "Sim7600ReceivedUssdTrigger",
    automation.Trigger.template(cg.std_string),
)

# Actions
Sim7600SendSmsAction = sim7600_ns.class_(
    "Sim7600SendSmsAction", automation.Action, cg.Component
)
Sim7600SendUssdAction = sim7600_ns.class_(
    "Sim7600SendUssdAction", automation.Action, cg.Component
)
Sim7600DialAction = sim7600_ns.class_("Sim7600DialAction", automation.Action, cg.Component)
Sim7600ConnectAction = sim7600_ns.class_(
    "Sim7600ConnectAction", automation.Action, cg.Component
)
Sim7600DisconnectAction = sim7600_ns.class_(
    "Sim7600DisconnectAction", automation.Action, cg.Component
)

CONF_SIM7600_ID = "sim7600_id"
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
            cv.GenerateID(): cv.declare_id(Sim7600Component),
            cv.Optional(CONF_ON_SMS_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim7600ReceivedMessageTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_INCOMING_CALL): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim7600IncomingCallTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_CALL_CONNECTED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim7600CallConnectedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_CALL_DISCONNECTED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim7600CallDisconnectedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_USSD_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim7600ReceivedUssdTrigger
                    ),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "sim7600", require_tx=True, require_rx=True
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


SIM7600_SEND_SMS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sim7600Component),
        cv.Required(CONF_RECIPIENT): cv.templatable(cv.string_strict),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "sim7600.send_sms", Sim7600SendSmsAction, SIM7600_SEND_SMS_SCHEMA
)
async def sim7600_send_sms_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    recipient_template = await cg.templatable(config[CONF_RECIPIENT], args, cg.std_string)
    cg.add(var.set_recipient(recipient_template))
    message_template = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(message_template))
    return var


SIM7600_DIAL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sim7600Component),
        cv.Required(CONF_RECIPIENT): cv.templatable(cv.string_strict),
    }
)


@automation.register_action("sim7600.dial", Sim7600DialAction, SIM7600_DIAL_SCHEMA)
async def sim7600_dial_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    recipient_template = await cg.templatable(config[CONF_RECIPIENT], args, cg.std_string)
    cg.add(var.set_recipient(recipient_template))
    return var


@automation.register_action(
    "sim7600.connect",
    Sim7600ConnectAction,
    cv.Schema({cv.GenerateID(): cv.use_id(Sim7600Component)}),
)
async def sim7600_connect_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


SIM7600_SEND_USSD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sim7600Component),
        cv.Required(CONF_USSD): cv.templatable(cv.string_strict),
    }
)


@automation.register_action(
    "sim7600.send_ussd", Sim7600SendUssdAction, SIM7600_SEND_USSD_SCHEMA
)
async def sim7600_send_ussd_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    ussd_template = await cg.templatable(config[CONF_USSD], args, cg.std_string)
    cg.add(var.set_ussd(ussd_template))
    return var


@automation.register_action(
    "sim7600.disconnect",
    Sim7600DisconnectAction,
    cv.Schema({cv.GenerateID(): cv.use_id(Sim7600Component)}),
)
async def sim7600_disconnect_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
