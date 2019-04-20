import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.automation import ACTION_REGISTRY
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE, UNIT_EMPTY, ICON_EMPTY
from .. import template_ns

TemplateSensor = template_ns.class_('TemplateSensor', sensor.PollingSensorComponent)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_EMPTY, ICON_EMPTY, 1).extend({
    cv.GenerateID(): cv.declare_id(TemplateSensor),
    cv.Optional(CONF_LAMBDA): cv.lambda_,
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [],
                                            return_type=cg.optional.template(float))
        cg.add(var.set_template(template_))


CONF_SENSOR_TEMPLATE_PUBLISH = 'sensor.template.publish'
SENSOR_TEMPLATE_PUBLISH_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
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
