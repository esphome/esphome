import esphome.codegen as cg
from esphome.components import uart

CODEOWNERS = ["@tuct", "@habbie"]
AUTO_LOAD = ["duty_cycle", "uart"]

pm100x_ns = cg.esphome_ns.namespace("pm100x")
PM100XComponent = pm100x_ns.class_(
    "PM100XComponent", uart.UARTDevice, cg.PollingComponent
)
