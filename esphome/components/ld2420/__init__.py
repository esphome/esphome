import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@descipher"]
DOMAIN = "ld2420"

DEPENDENCIES = ["uart"]

MULTI_CONF = True

ld2420_ns = cg.esphome_ns.namespace("ld2420")
LD2420Component = ld2420_ns.class_("LD2420Component", cg.Component, uart.UARTDevice)

CONF_LD2420_ID = "ld2420_id"

_register_listener = cg.slot_list("LD2420_LISTENER_COUNT", "register_listener")


async def register_listener(hub: cg.MockObj, var: cg.MockObj) -> None:
    cg.add(_register_listener(hub, var))


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2420Component),
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


async def to_code(config: ConfigType) -> None:
    await uart.new_uart_component(config)
