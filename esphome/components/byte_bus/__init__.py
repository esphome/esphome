import esphome.codegen as cg

CODEOWNERS = ["@clydebarrow"]

byte_bus_ns = cg.esphome_ns.namespace("byte_bus")
ByteBus = byte_bus_ns.class_("ByteBus")
