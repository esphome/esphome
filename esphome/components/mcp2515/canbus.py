import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, binary_sensor, canbus
from esphome.const import CONF_DC_PIN, CONF_CS_PIN, CONF_ID

DEPENDENCIES = ['spi']

mcp2515_ns = cg.esphome_ns.namespace('mcp2515')
mcp2515 = mcp2515_ns.class_('MCP2515', cg.Component, canbus.CanbusComponent, spi.SPIDevice)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(mcp2515),
    cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
}).extend(spi.SPI_DEVICE_SCHEMA)


def to_code(config):
    rhs = mcp2515.new()
    var = cg.Pvariable(config[CONF_ID], rhs)

    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)
    dc = yield cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
    cs = yield cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs))
