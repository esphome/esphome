import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart
from esphome.const import CONF_ID, CONF_ON_TAG, CONF_TRIGGER_ID

CODEOWNERS = ["@Swamp-Ig"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor"]
MULTI_CONF = True

ws18x0_uart_ns = cg.esphome_ns.namespace("ws18x0_uart")
WS18x0UARTComponent = ws18x0_uart_ns.class_(
    "WS18x0UARTComponent", cg.Component, uart.UARTDevice
)
WS18x0UARTTrigger = ws18x0_uart_ns.class_(
    "WS18x0UARTTrigger", automation.Trigger.template(cg.uint32)
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WS18x0UARTComponent),
            cv.Optional(CONF_ON_TAG): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(WS18x0UARTTrigger),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_trigger(trigger))
        await automation.build_automation(trigger, [(cg.uint32, "x")], conf)
