import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_STARTUP_DELAY


CODEOWNERS = ["@descipher"]

DEPENDENCIES = ["uart"]

MULTI_CONF = True

ld2420_ns = cg.esphome_ns.namespace("ld2420")
LD2420Component = ld2420_ns.class_("LD2420Component", cg.Component, uart.UARTDevice)

CONF_LD2420_ID = "ld2420_id"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2420Component),
            cv.Optional(
                CONF_STARTUP_DELAY, default="0s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld2420_uart",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY]))
