import esphome.codegen as cg
from esphome.components import pm100x, uart

pm100x_uart_ns = cg.esphome_ns.namespace("pm100x_uart")
PM100XComponentUART = pm100x_uart_ns.class_(
    "PM100XComponentUART", pm100x.PM100XComponent, uart.UARTDevice
)
