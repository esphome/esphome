import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_THROTTLE

AUTO_LOAD = ["ld24xx"]
CODEOWNERS = ["@Rihan9"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

LD2412_ns = cg.esphome_ns.namespace("ld2412")
LD2412Component = LD2412_ns.class_("LD2412Component", cg.Component, uart.UARTDevice)

CONF_LD2412_ID = "ld2412_id"

CONF_MAX_MOVE_DISTANCE = "max_move_distance"
CONF_MAX_STILL_DISTANCE = "max_still_distance"
CONF_MOVE_THRESHOLDS = [f"g{x}_move_threshold" for x in range(9)]
CONF_STILL_THRESHOLDS = [f"g{x}_still_threshold" for x in range(9)]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2412Component),
            cv.Optional(CONF_THROTTLE): cv.invalid(
                f"{CONF_THROTTLE} has been removed; use per-sensor filters, instead"
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld2412",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
