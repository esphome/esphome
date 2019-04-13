import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import spi
from esphome.components.spi import SPIComponent
from esphome.const import CONF_CS_PIN, CONF_ID, CONF_ON_TAG, CONF_SPI_ID, CONF_TRIGGER_ID, \
    CONF_UPDATE_INTERVAL

DEPENDENCIES = ['spi']
AUTO_LOAD = ['binary_sensor']
MULTI_CONF = True

pn532_ns = cg.esphome_ns.namespace('pn532')
PN532 = pn532_ns.class_('PN532', cg.PollingComponent, spi.SPIDevice)
PN532Trigger = pn532_ns.class_('PN532Trigger', cg.Trigger.template(cg.std_string))

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(PN532),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_UPDATE_INTERVAL, default='1s'): cv.update_interval,
    cv.Optional(CONF_ON_TAG): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(PN532Trigger),
    }),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    spi_ = yield cg.get_variable(config[CONF_SPI_ID])
    cs = yield cg.gpio_pin_expression(config[CONF_CS_PIN])
    var = cg.new_Pvariable(config[CONF_ID], spi_, cs, config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_trigger(trigger))
        yield automation.build_automation(trigger, [(cg.std_string, 'x')], conf)
