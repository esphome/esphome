import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import spi
from esphome.const import CONF_ID, CONF_ON_TAG, CONF_TRIGGER_ID

DEPENDENCIES = ['spi']
AUTO_LOAD = ['binary_sensor']
MULTI_CONF = True

pn532_ns = cg.esphome_ns.namespace('pn532')
PN532 = pn532_ns.class_('PN532', cg.PollingComponent, spi.SPIDevice)
PN532Trigger = pn532_ns.class_('PN532Trigger', automation.Trigger.template(cg.std_string))

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(PN532),
    cv.Optional(CONF_ON_TAG): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PN532Trigger),
    }),
}).extend(cv.polling_component_schema('1s')).extend(spi.SPI_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_trigger(trigger))
        yield automation.build_automation(trigger, [(cg.std_string, 'x')], conf)
