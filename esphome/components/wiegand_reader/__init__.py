import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import CONF_ID, CONF_ON_TAG, CONF_TRIGGER_ID, CONF_D0_PIN, CONF_D1_PIN

CODEOWNERS = ['@reidprojects']
DEPENDENCIES = []
AUTO_LOAD = []
MULTI_CONF = True

wiegand_reader_ns = cg.esphome_ns.namespace('wiegand_reader')
WiegandReader = wiegand_reader_ns.class_('WiegandReader', cg.PollingComponent)
WiegandReaderTrigger = wiegand_reader_ns.class_('WiegandReaderTrigger', automation.Trigger.template(cg.std_string))

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WiegandReader),
    cv.Required(CONF_D0_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_D1_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_ON_TAG): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(WiegandReaderTrigger),
    }),
}).extend(cv.polling_component_schema('1s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    d0_pin = yield cg.gpio_pin_expression(config[CONF_D0_PIN])
    d1_pin = yield cg.gpio_pin_expression(config[CONF_D1_PIN])
    cg.add(var.set_data_pins(d0_pin, d1_pin))
    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_trigger(trigger))
        yield automation.build_automation(trigger, [(cg.std_string, 'x')], conf)

    cg.add_library('paulo-raca/Yet Another Arduino Wiegand Library', '2.0.0')
