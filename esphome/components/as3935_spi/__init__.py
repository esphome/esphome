import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import as3935_base, spi
from esphome.const import CONF_DC_PIN, CONF_ID, CONF_LAMBDA, CONF_PAGES

AUTO_LOAD = ['as3935_base']
DEPENDENCIES = ['spi']

as3935_spi_ns = cg.esphome_ns.namespace('as3935_spi')
SPIAS3935 = as3935_spi_ns.class_('SPIAS3935Component', as3935_base.AS3935, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(as3935_base.AS3935_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(SPIAS3935)
}).extend(cv.COMPONENT_SCHEMA).extend(spi.SPI_DEVICE_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield as3935_base.setup_as3935(var, config)
    yield spi.register_spi_device(var, config)
