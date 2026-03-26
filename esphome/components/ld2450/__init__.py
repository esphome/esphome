from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_DATA, CONF_THROTTLE, CONF_TRIGGER_ID

AUTO_LOAD = ["ld24xx"]
DEPENDENCIES = ["uart"]
CODEOWNERS = ["@hareeshmu"]
MULTI_CONF = True

ld2450_ns = cg.esphome_ns.namespace("ld2450")
LD2450Component = ld2450_ns.class_("LD2450Component", cg.Component, uart.UARTDevice)

LD2450DataTrigger = ld2450_ns.class_("LD2450DataTrigger", automation.Trigger.template())

CONF_LD2450_ID = "ld2450_id"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2450Component),
            cv.Optional(CONF_THROTTLE): cv.invalid(
                f"{CONF_THROTTLE} has been removed; use per-sensor filters, instead"
            ),
            cv.Optional(CONF_ON_DATA): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LD2450DataTrigger),
                }
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

LD2450BaseSchema = cv.Schema(
    {
        cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    },
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld2450",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    for conf in config.get(CONF_ON_DATA, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
