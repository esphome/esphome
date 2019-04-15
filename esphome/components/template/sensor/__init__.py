
from esphome.automation import ACTION_REGISTRY
from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_STATE, CONF_UPDATE_INTERVAL
from .. import template_ns

TemplateSensor = template_ns.class_('TemplateSensor', sensor.PollingSensorComponent)

CONFIG_SCHEMA = cv.nameable(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateSensor),
    cv.Optional(CONF_LAMBDA): cv.lambda_,
    cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    rhs = TemplateSensor.new(config[CONF_NAME], config[CONF_UPDATE_INTERVAL])
    template = cg.Pvariable(config[CONF_ID], rhs)
    yield cg.register_component(template, config)
    yield sensor.register_sensor(template, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [],
                                            return_type=cg.optional.template(float))
        cg.add(template.set_template(template_))


CONF_SENSOR_TEMPLATE_PUBLISH = 'sensor.template.publish'
SENSOR_TEMPLATE_PUBLISH_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_variable_id(sensor.Sensor),
    cv.Required(CONF_STATE): cv.templatable(cv.float_),
})


@ACTION_REGISTRY.register(CONF_SENSOR_TEMPLATE_PUBLISH, SENSOR_TEMPLATE_PUBLISH_ACTION_SCHEMA)
def sensor_template_publish_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = sensor.SensorPublishAction.template(template_arg)
    rhs = type.new(var)
    action = cg.Pvariable(action_id, rhs, type=type)
    template_ = yield cg.templatable(config[CONF_STATE], args, float)
    cg.add(action.set_state(template_))
    yield action
