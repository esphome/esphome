import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE
from .. import template_ns

TemplateBinarySensor = template_ns.class_('TemplateBinarySensor', binary_sensor.BinarySensor,
                                          cg.Component)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(TemplateBinarySensor),
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield binary_sensor.register_binary_sensor(var, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [],
                                            return_type=cg.optional.template(bool))
        cg.add(var.set_template(template_))


@automation.register_action('binary_sensor.template.publish',
                            binary_sensor.BinarySensorPublishAction,
                            cv.Schema({
                                cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
                                cv.Required(CONF_STATE): cv.templatable(cv.boolean),
                            }))
def binary_sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))
    yield var
