import esphome.codegen as cg
from esphome.components import uart

CODEOWNERS = ["@functionpointer"]
DEPENDENCIES = ["uart"]

hydreon_rgxx_ns = cg.esphome_ns.namespace("hydreon_rgxx")
RGModel = hydreon_rgxx_ns.enum("RGModel")
RG15Resolution = hydreon_rgxx_ns.enum("RG15Resolution")
HydreonRGxxComponent = hydreon_rgxx_ns.class_(
    "HydreonRGxxComponent", cg.PollingComponent, uart.UARTDevice
)
