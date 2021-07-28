import esphome.codegen as cg
from esphome.components import uart

nextion_ns = cg.esphome_ns.namespace("nextion")
Nextion = nextion_ns.class_("Nextion", cg.PollingComponent, uart.UARTDevice)
nextion_ref = Nextion.operator("ref")

CONF_NEXTION_ID = "nextion_id"
