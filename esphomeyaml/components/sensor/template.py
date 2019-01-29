import voluptuous as vol

from esphomeyaml.automation import ACTION_REGISTRY
from esphomeyaml.components import sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL, CONF_ID, \
    CONF_STATE
from esphomeyaml.cpp_generator import add, process_lambda, variable, get_variable, Pvariable, \
    templatable
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App, Application, float_, optional, Action

MakeTemplateSensor = Application.struct('MakeTemplateSensor')
TemplateSensor = sensor.sensor_ns.class_('TemplateSensor', sensor.PollingSensorComponent)
SensorPublishAction = sensor.sensor_ns.class_('SensorPublishAction', Action)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateSensor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_template_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    template = make.Ptemplate_

    sensor.setup_sensor(template, make.Pmqtt, config)
    setup_component(template, config)

    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=optional.template(float_)):
        yield
    add(template.set_template(template_))


BUILD_FLAGS = '-DUSE_TEMPLATE_SENSOR'


CONF_SENSOR_TEMPLATE_PUBLISH = 'sensor.template.publish'
SENSOR_TEMPLATE_PUBLISH_ACTION_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.use_variable_id(sensor.Sensor),
    vol.Required(CONF_STATE): cv.templatable(cv.float_),
})


@ACTION_REGISTRY.register(CONF_SENSOR_TEMPLATE_PUBLISH, SENSOR_TEMPLATE_PUBLISH_ACTION_SCHEMA)
def sensor_template_publish_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_sensor_publish_action(template_arg)
    type = SensorPublishAction.template(arg_type)
    action = Pvariable(action_id, rhs, type=type)
    for template_ in templatable(config[CONF_STATE], arg_type, float_):
        yield None
    add(action.set_state(template_))
    yield action


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
