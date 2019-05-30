import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import text_sensor
from esphome.components.text_sensor import TextSensorPublishAction
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE
from .. import template_ns

TemplateTextSensor = template_ns.class_('TemplateTextSensor', text_sensor.TextSensor,
                                        cg.PollingComponent)

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(TemplateTextSensor),
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield text_sensor.register_text_sensor(var, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [],
                                            return_type=cg.optional.template(cg.std_string))
        cg.add(var.set_template(template_))


@automation.register_action('text_sensor.template.publish', TextSensorPublishAction, cv.Schema({
    cv.Required(CONF_ID): cv.use_id(text_sensor.TextSensor),
    cv.Required(CONF_STATE): cv.templatable(cv.string_strict),
}))
def text_sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_STATE], args, cg.std_string)
    cg.add(var.set_state(template_))
    yield var
