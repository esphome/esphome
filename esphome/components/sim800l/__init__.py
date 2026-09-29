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


automation.register_apply_action(
    "sim800l.send_sms",
    SIM800L_SEND_SMS_SCHEMA,
    automation.ApplyCall(
        "send_sms({}, {})",
        ((CONF_RECIPIENT, cg.std_string), (CONF_MESSAGE, cg.std_string)),
    ),
)

SIM800L_DIAL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sim800LComponent),
        cv.Required(CONF_RECIPIENT): cv.templatable(cv.string_strict),
    }
)

automation.register_apply_action(
    "sim800l.dial",
    SIM800L_DIAL_SCHEMA,
    automation.ApplyField(CONF_RECIPIENT, "dial", cg.std_string),
)

SIM800L_SEND_USSD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sim800LComponent),
        cv.Required(CONF_USSD): cv.templatable(cv.string_strict),
    }
)

automation.register_apply_action(
    "sim800l.send_ussd",
    SIM800L_SEND_USSD_SCHEMA,
    automation.ApplyField(CONF_USSD, "send_ussd", cg.std_string),
)

SIM800L_ID_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(Sim800LComponent)})

automation.register_apply_action(
    "sim800l.connect", SIM800L_ID_SCHEMA, automation.ApplyCall("connect()")
)
automation.register_apply_action(
    "sim800l.disconnect", SIM800L_ID_SCHEMA, automation.ApplyCall("disconnect()")
)
