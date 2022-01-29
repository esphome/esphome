import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ID,
    CONF_TRIGGER_ID,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)
from esphome.components import binary_sensor, sensor, uart

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@glmnet"]
MULTI_CONF = True

sim800l_ns = cg.esphome_ns.namespace("sim800l")
Sim800LComponent = sim800l_ns.class_("Sim800LComponent", cg.Component)

Sim800LReceivedMessageTrigger = sim800l_ns.class_(
    "Sim800LReceivedMessageTrigger",
    automation.Trigger.template(cg.std_string, cg.std_string),
)

# Actions
Sim800LSendSmsAction = sim800l_ns.class_("Sim800LSendSmsAction", automation.Action)
Sim800LDialAction = sim800l_ns.class_("Sim800LDialAction", automation.Action)

CONF_ON_SMS_RECEIVED = "on_sms_received"
CONF_RECIPIENT = "recipient"
CONF_REGISTERED = "registered"
CONF_RSSI = "rssi"
CONF_MESSAGE = "message"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Sim800LComponent),
            cv.Optional(CONF_REGISTERED): binary_sensor.BINARY_SENSOR_SCHEMA.extend(
                {
                    cv.Optional(
                        CONF_DEVICE_CLASS, default=DEVICE_CLASS_CONNECTIVITY
                    ): binary_sensor.device_class,
                    cv.Optional(
                        CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_DIAGNOSTIC
                    ): cv.entity_category,
                }
            ),
            cv.Optional(CONF_RSSI): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_ON_SMS_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Sim800LReceivedMessageTrigger
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

    if CONF_REGISTERED in config:
        conf = config[CONF_REGISTERED]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_registered(sens))

    if CONF_RSSI in config:
        conf = config[CONF_RSSI]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_rssi_sensor(sens))

    for conf in config.get(CONF_ON_SMS_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "message"), (cg.std_string, "sender")], conf
        )


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
