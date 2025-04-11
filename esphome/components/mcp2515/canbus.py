import esphome.codegen as cg
from esphome.components import canbus, spi
from esphome.components.canbus import CanbusComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE

CODEOWNERS = ["@mvturnho", "@danielschramm"]
DEPENDENCIES = ["spi"]

CONF_CLOCK = "clock"

mcp2515_ns = cg.esphome_ns.namespace("mcp2515")
mcp2515 = mcp2515_ns.class_("MCP2515", CanbusComponent, spi.SPIDevice)
CanClock = mcp2515_ns.enum("CAN_CLOCK")
McpMode = mcp2515_ns.enum("CANCTRL_REQOP_MODE")

CAN_CLOCK = {
    "8MHZ": CanClock.MCP_8MHZ,
    "12MHZ": CanClock.MCP_12MHZ,
    "16MHZ": CanClock.MCP_16MHZ,
    "20MHZ": CanClock.MCP_20MHZ,
}

MCP_MODE = {
    "NORMAL": McpMode.CANCTRL_REQOP_NORMAL,
    "LOOPBACK": McpMode.CANCTRL_REQOP_LOOPBACK,
    "LISTENONLY": McpMode.CANCTRL_REQOP_LISTENONLY,
}

CONFIG_SCHEMA = canbus.CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(mcp2515),
        cv.Optional(CONF_CLOCK, default="8MHZ"): cv.enum(CAN_CLOCK, upper=True),
        cv.Optional(CONF_MODE, default="NORMAL"): cv.enum(MCP_MODE, upper=True),
    }
).extend(spi.spi_device_schema(True))


async def to_code(config):
    rhs = mcp2515.new()
    var = cg.Pvariable(config[CONF_ID], rhs)
    await canbus.register_canbus(var, config)
    if CONF_CLOCK in config:
        canclock = CAN_CLOCK[config[CONF_CLOCK]]
        cg.add(var.set_mcp_clock(canclock))
    if CONF_MODE in config:
        mode = MCP_MODE[config[CONF_MODE]]
        cg.add(var.set_mcp_mode(mode))

    await spi.register_spi_device(var, config)
