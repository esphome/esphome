import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, pn532
from esphome.const import CONF_ID

AUTO_LOAD = ['pn532']
CODEOWNERS = ['@OttoWinter', '@jesserockz']
DEPENDENCIES = ['spi']

pn532_spi_ns = cg.esphome_ns.namespace('pn532_spi')
PN532Spi = pn532_spi_ns.class_('PN532Spi', pn532.PN532, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(pn532.PN532_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(PN532Spi),
}).extend(spi.spi_device_schema(cs_pin_required=True)))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield pn532.setup_pn532(var, config)
    yield spi.register_spi_device(var, config)
