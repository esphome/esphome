import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_THROTTLE,
)

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@hareeshmu"]
MULTI_CONF = True

ld2450_ns = cg.esphome_ns.namespace("ld2450")
LD2450Component = ld2450_ns.class_("LD2450Component", cg.Component, uart.UARTDevice)

CONF_LD2450_ID = "ld2450_id"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2450Component),
            cv.Optional(CONF_THROTTLE, default="1000ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=1)),
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
    cg.add(var.set_throttle(config[CONF_THROTTLE]))
