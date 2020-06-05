import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import CONF_BUSY_PIN, CONF_DC_PIN, CONF_FULL_UPDATE_EVERY, \
    CONF_ID, CONF_LAMBDA, CONF_MODEL, CONF_PAGES, CONF_RESET_PIN


pxmatrix_ns = cg.esphome_ns.namespace('pxmatrix_display')

pxmatrix_gpio = pxmatrix_ns.class_('PxmatrixDisplay', cg.PollingComponent, display.DisplayBuffer)


CONFIG_SCHEMA = cv.All(display.FULL_DISPLAY_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(pxmatrix_gpio),
}).extend(cv.polling_component_schema('1ms')),
                       cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    yield cg.register_component(var, config)
    yield display.register_display(var, config)

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')], return_type=cg.void)
        cg.add(var.set_writer(lambda_))