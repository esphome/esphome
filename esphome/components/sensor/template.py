import voluptuous as vol

from esphome.automation import ACTION_REGISTRY
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_STATE, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add, get_variable, process_lambda, templatable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import Action, App, float_, optional

TemplateSensor = sensor.sensor_ns.class_('TemplateSensor', sensor.PollingSensorComponent)
SensorPublishAction = sensor.sensor_ns.class_('SensorPublishAction', Action)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateSensor),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_template_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    template = Pvariable(config[CONF_ID], rhs)

    sensor.setup_sensor(template, config)
    setup_component(template, config)

    if CONF_LAMBDA in config:
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
