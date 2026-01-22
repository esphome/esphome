from esphome import automation
import esphome.codegen as cg
from esphome.components import uart, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_DATA,
    CONF_ID,
    CONF_TRIGGER_ID,
    CONF_WEB_SERVER,
)
from esphome.core.entity_helpers import entity_duplicate_validator, setup_entity

# This component could be designed as a platform, but since it requires some kind of uart component,
# and ESPHome's UART component is not implemented be used as a platform, we implement it
# directly as a component that requires a pre-configured uart.

CODEOWNERS = ["@clydebarrow"]

serial_channel_ns = cg.esphome_ns.namespace("serial_channel")
SerialChannel = serial_channel_ns.class_(
    "SerialChannel", cg.EntityBase, uart.UARTDevice, cg.Component
)
SerialChannelPtr = SerialChannel.operator("ptr")

# Triggers
CONF_ON_DATA = "on_data"
SerialChannelStateTrigger = serial_channel_ns.class_(
    "SerialChannelStateTrigger",
    automation.Trigger.template(cg.std_string),
)

# Actions
SerialChannelSendAction = serial_channel_ns.class_(
    "SerialChannelSendAction", automation.Action
)

CONFIG_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SerialChannel),
            cv.Optional(CONF_BUFFER_SIZE, default=256): cv.positive_int,
            cv.Optional(CONF_ON_DATA): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        SerialChannelStateTrigger
                    ),
                }
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

CONFIG_SCHEMA.add_extra(entity_duplicate_validator("serial_channel"))


async def to_code(config):
    cg.add_define("USE_SERIAL_CHANNEL")
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_BUFFER_SIZE])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await setup_entity(var, config, "serial_channel")

    cg.add(cg.App.register_serial_channel(var))

    for conf in config.get(CONF_ON_DATA, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)


OPERATION_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(SerialChannel),
    }
)


@automation.register_action(
    "serial_channel.send",
    SerialChannelSendAction,
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_DATA): cv.templatable(cv.string_strict),
        }
    ),
)
async def serial_channel_send_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_DATA], args, cg.std_string)
    cg.add(var.set_data(template_))
    return var
