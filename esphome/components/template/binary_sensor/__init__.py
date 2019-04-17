import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.automation import ACTION_REGISTRY
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_STATE
from .. import template_ns

TemplateBinarySensor = template_ns.class_('TemplateBinarySensor', binary_sensor.BinarySensor,
                                          cg.Component)

CONFIG_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateBinarySensor),
    cv.Optional(CONF_LAMBDA): cv.lambda_,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    yield cg.register_component(var, config)
    yield binary_sensor.register_binary_sensor(var, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [],
                                            return_type=cg.optional.template(bool))
        cg.add(var.set_template(template_))


@ACTION_REGISTRY.register('binary_sensor.template.publish', cv.Schema({
    cv.Required(CONF_ID): cv.use_variable_id(binary_sensor.BinarySensor),
    cv.Required(CONF_STATE): cv.templatable(cv.boolean),
}))
def binary_sensor_template_publish_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = binary_sensor.BinarySensorPublishAction.template(template_arg)
    rhs = type.new(var)
    action = cg.Pvariable(action_id, rhs, type=type)
    template_ = yield cg.templatable(config[CONF_STATE], args, bool)
    cg.add(action.set_state(template_))
    yield action
