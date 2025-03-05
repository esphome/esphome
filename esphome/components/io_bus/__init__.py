import esphome.codegen as cg

io_bus_ns = cg.esphome_ns.namespace("io_bus")
IOBus = io_bus_ns.class_("IOBus")

CODEOWNERS = ["@clydebarrow"]
# Allow configuration in yaml, for testing
CONFIG_SCHEMA = {}
