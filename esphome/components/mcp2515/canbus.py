import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, canbus
from esphome.automation import LambdaAction
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome.components.canbus import CanbusComponent,CONF_ON_RECEIVE, CONF_CAN_ID

print("mcp2515.canbus.py")
AUTO_LOAD = ['canbus']
DEPENDENCIES = ['spi']

mcp2515_ns = cg.esphome_ns.namespace('mcp2515')
mcp2515 = mcp2515_ns.class_('MCP2515', CanbusComponent, spi.SPIDevice)

CONFIG_SCHEMA = canbus.CONFIG_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(mcp2515),
}).extend(spi.SPI_DEVICE_SCHEMA)


def to_code(config):
    rhs = mcp2515.new()
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield canbus.register_canbus(var, config)
    yield spi.register_spi_device(var, config)
    for conf in config.get(CONF_ON_RECEIVE, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], conf[CONF_CAN_ID])
        yield cg.register_component(trig, conf)
        yield automation.build_automation(trig, [(cg.std_string, 'x')], conf)
