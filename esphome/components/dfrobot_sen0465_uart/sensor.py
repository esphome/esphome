import esphome.codegen as cg
from esphome.components import uart
from esphome.components.dfrobot_sen0465_base import (
    DFRobotSEN0465Component,
    dfrobot_sen0465_schema,
    to_code_base,
)
from esphome.types import ConfigType

from . import DOMAIN

AUTO_LOAD = ["dfrobot_sen0465_base"]
DEPENDENCIES = ["uart"]

dfrobot_sen0465_uart_ns = cg.esphome_ns.namespace("dfrobot_sen0465_uart")
DFRobotSEN0465UARTComponent = dfrobot_sen0465_uart_ns.class_(
    "DFRobotSEN0465UARTComponent", DFRobotSEN0465Component, uart.UARTDevice
)

CONFIG_SCHEMA = dfrobot_sen0465_schema(
    DFRobotSEN0465UARTComponent, uart.UART_DEVICE_SCHEMA
)

# The sensor's baud rate is fixed
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=9600,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = await to_code_base(config)
    await uart.register_uart_device(var, config)
