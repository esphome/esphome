from esphome.automation import ACTION_REGISTRY
from esphome.components import text_sensor
from esphome.components.text_sensor import TextSensorPublishAction
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_STATE, CONF_UPDATE_INTERVAL


TemplateTextSensor = text_sensor.text_sensor_ns.class_('TemplateTextSensor',
                                                       text_sensor.TextSensor, PollingComponent)

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateTextSensor),
    cv.Optional(CONF_LAMBDA): cv.lambda_,
    cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    rhs = App.make_template_text_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    template = Pvariable(config[CONF_ID], rhs)
    text_sensor.setup_text_sensor(template, config)
    register_component(template, config)

    if CONF_LAMBDA in config:
        template_ = yield process_lambda(config[CONF_LAMBDA], [],
                                         return_type=optional.template(std_string))
        cg.add(template.set_template(template_))


BUILD_FLAGS = '-DUSE_TEMPLATE_TEXT_SENSOR'

CONF_TEXT_SENSOR_TEMPLATE_PUBLISH = 'text_sensor.template.publish'
TEXT_SENSOR_TEMPLATE_PUBLISH_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_variable_id(text_sensor.TextSensor),
    cv.Required(CONF_STATE): cv.templatable(cv.string_strict),
})


@ACTION_REGISTRY.register(CONF_TEXT_SENSOR_TEMPLATE_PUBLISH,
                          TEXT_SENSOR_TEMPLATE_PUBLISH_ACTION_SCHEMA)
def text_sensor_template_publish_to_code(config, action_id, template_arg, args):
    var = yield get_variable(config[CONF_ID])
    rhs = var.make_text_sensor_publish_action(template_arg)
    type = TextSensorPublishAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    template_ = yield templatable(config[CONF_STATE], args, std_string)
    cg.add(action.set_state(template_))
    yield action
