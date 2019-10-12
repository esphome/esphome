import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import as3935, spi
from esphome.const import CONF_ID

AUTO_LOAD = ['as3935']
DEPENDENCIES = ['spi']

as3935_spi_ns = cg.esphome_ns.namespace('as3935_spi')
SPIAS3935 = as3935_spi_ns.class_('SPIAS3935Component', as3935.AS3935, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(as3935.AS3935_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(SPIAS3935)
}).extend(cv.COMPONENT_SCHEMA).extend(spi.SPI_DEVICE_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield as3935.setup_as3935(var, config)
    yield spi.register_spi_device(var, config)
