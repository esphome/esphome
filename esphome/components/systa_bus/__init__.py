import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@Mat931"]

DEPENDENCIES = ["uart"]

MULTI_CONF = True

systa_bus_ns = cg.esphome_ns.namespace("systa_bus")
SystaBus = systa_bus_ns.class_("SystaBus", cg.Component, uart.UARTDevice)

CONF_SYSTA_BUS_ID = "systa_bus_id"

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SystaBus),
    }
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "systa_bus", baud_rate=9600, require_rx=True
)

_request_listener_slot = cg.slot_counter("SYSTA_BUS_LISTENER_COUNT")


async def register_systa_bus_listener(systa_bus: MockObj, var: MockObj) -> None:
    """Register a listener with its bus and count it for the compile-time listener storage."""
    _request_listener_slot(str(systa_bus))
    cg.add(systa_bus.register_listener(var))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
