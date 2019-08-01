import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, canbus
from esphome.const import CONF_ID
from esphome.components.canbus import CanbusComponent, CAN_SPEEDS, CONF_BIT_RATE

AUTO_LOAD = ['canbus']
DEPENDENCIES = ['spi']

CONF_MCP_CLOCK = 'clock'

mcp2515_ns = cg.esphome_ns.namespace('mcp2515')
mcp2515 = mcp2515_ns.class_('MCP2515', CanbusComponent, spi.SPIDevice)
CanClock = mcp2515_ns.enum('CAN_CLOCK')
CAN_CLOCK = {
    '20MHZ': CanClock.MCP_20MHZ,
    '16MHZ': CanClock.MCP_16MHZ,
    '8MHZ':CanClock.MCP_8MHZ,
}

CONFIG_SCHEMA = canbus.CONFIG_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(mcp2515),
    cv.Optional(CONF_MCP_CLOCK, default='8MHZ'): cv.enum(CAN_CLOCK, upper=True),
}).extend(spi.SPI_DEVICE_SCHEMA)


def to_code(config):
    rhs = mcp2515.new()
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield canbus.register_canbus(var, config)
    # canclock = CAN_CLOCK[config[CONF_MCP_CLOCK]]
    # bitrate = CAN_SPEEDS[config[CONF_BIT_RATE]]
    # cg.add(var.set_bitrate(bitrate,canclock))

    yield spi.register_spi_device(var, config)

