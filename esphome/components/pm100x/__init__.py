import esphome.codegen as cg
from esphome.components import uart

CODEOWNERS = ["@tuct", "@habbie"]

pm100x_ns = cg.esphome_ns.namespace("pm100x")
PM100XComponent = pm100x_ns.class_("PM100XComponent", cg.PollingComponent)
PM100XComponentUART = pm100x_ns.class_(
    "PM100XComponentUART", PM100XComponent, uart.UARTDevice
)
