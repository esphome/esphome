from esphome.automation import ACTION_REGISTRY
from esphome.components import text_sensor
from esphome.components.text_sensor import TextSensorPublishAction
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_STATE, CONF_UPDATE_INTERVAL
from .. import template_ns


TemplateTextSensor = template_ns.class_('TemplateTextSensor', text_sensor.TextSensor,
                                        cg.PollingComponent)

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateTextSensor),
    cv.Optional(CONF_LAMBDA): cv.lambda_,
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)
    yield text_sensor.register_text_sensor(var, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [],
                                            return_type=cg.optional.template(cg.std_string))
        cg.add(var.set_template(template_))


@ACTION_REGISTRY.register('text_sensor.template.publish', cv.Schema({
    cv.Required(CONF_ID): cv.use_variable_id(text_sensor.TextSensor),
    cv.Required(CONF_STATE): cv.templatable(cv.string_strict),
}))
def text_sensor_template_publish_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = TextSensorPublishAction.template(template_arg)
    rhs = type.new(var)
    action = cg.Pvariable(action_id, rhs, type=type)
    template_ = yield cg.templatable(config[CONF_STATE], args, cg.std_string)
    cg.add(action.set_state(template_))
    yield action
