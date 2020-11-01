import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ON_TAG, CONF_TRIGGER_ID
from esphome.core import coroutine

CODEOWNERS = ['@OttoWinter', '@jesserockz']
AUTO_LOAD = ['binary_sensor']
MULTI_CONF = True

CONF_PN532_ID = 'pn532_id'

pn532_ns = cg.esphome_ns.namespace('pn532')
PN532 = pn532_ns.class_('PN532', cg.PollingComponent)

PN532Trigger = pn532_ns.class_('PN532Trigger', automation.Trigger.template(cg.std_string))

PN532_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(PN532),
    cv.Optional(CONF_ON_TAG): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PN532Trigger),
    }),
}).extend(cv.polling_component_schema('1s'))


def CONFIG_SCHEMA(conf):
    if conf:
        raise cv.Invalid("This component has been moved in 1.16, please see the docs for updated "
                         "instructions. https://esphome.io/components/binary_sensor/pn532.html")


@coroutine
def setup_pn532(var, config):
    yield cg.register_component(var, config)

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_trigger(trigger))
        yield automation.build_automation(trigger, [(cg.std_string, 'x')], conf)
