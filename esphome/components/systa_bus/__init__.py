import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@Mat931"]
DOMAIN = "systa_bus"

DEPENDENCIES = ["uart"]

MULTI_CONF = True

systa_bus_ns = cg.esphome_ns.namespace("systa_bus")
SystaBus = systa_bus_ns.class_("SystaBus", cg.Component, uart.UARTDevice)

CONF_SYSTA_BUS_ID = "systa_bus_id"

CONF_SYSTASOLAR_AQUA = "systasolar_aqua"

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SystaBus),
    }
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "systa_bus", baud_rate=9600, require_rx=True
)

_register_listener = cg.slot_list("SYSTA_BUS_LISTENER_COUNT", "register_listener")


async def register_systa_bus_listener(systa_bus: MockObj, var: MockObj) -> None:
    """Register a listener with its bus."""
    cg.add(_register_listener(systa_bus, var))


async def to_code(config: ConfigType) -> None:
    await uart.new_uart_component(config)
