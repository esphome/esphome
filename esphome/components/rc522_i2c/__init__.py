import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_ON_TAG, CONF_TRIGGER_ID, CONF_RESET_PIN, CONF_CS_PIN

CODEOWNERS = ['@glmnet']
DEPENDENCIES = ['i2c']
AUTO_LOAD = ['binary_sensor']
MULTI_CONF = True


rc522_ns = cg.esphome_ns.namespace('rc522_i2c')
RC522 = rc522_ns.class_('RC522', cg.PollingComponent, i2c.I2CDevice)
RC522Trigger = rc522_ns.class_('RC522Trigger', automation.Trigger.template(cg.std_string))

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(RC522),
    cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_ON_TAG): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RC522Trigger),
    }),
}).extend(cv.polling_component_schema('1s')).extend(i2c.i2c_device_schema())


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_RESET_PIN in config:
        reset = yield cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_trigger(trigger))
        yield automation.build_automation(trigger, [(cg.std_string, 'x')], conf)
