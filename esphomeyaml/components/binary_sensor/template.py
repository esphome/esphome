import voluptuous as vol

from esphomeyaml.automation import ACTION_REGISTRY
from esphomeyaml.components import binary_sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_ID, CONF_STATE
from esphomeyaml.cpp_generator import variable, process_lambda, add, get_variable, Pvariable, \
    templatable
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import Application, Component, App, optional, bool_, Action

MakeTemplateBinarySensor = Application.struct('MakeTemplateBinarySensor')
TemplateBinarySensor = binary_sensor.binary_sensor_ns.class_('TemplateBinarySensor',
                                                             binary_sensor.BinarySensor,
                                                             Component)
BinarySensorPublishAction = binary_sensor.binary_sensor_ns.class_('BinarySensorPublishAction',
                                                                  Action)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateBinarySensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateBinarySensor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_template_binary_sensor(config[CONF_NAME])
    make = variable(config[CONF_MAKE_ID], rhs)
    binary_sensor.setup_binary_sensor(make.Ptemplate_, make.Pmqtt, config)
    setup_component(make.Ptemplate_, config)

    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=optional.template(bool_)):
        yield
    add(make.Ptemplate_.set_template(template_))


BUILD_FLAGS = '-DUSE_TEMPLATE_BINARY_SENSOR'


CONF_BINARY_SENSOR_TEMPLATE_PUBLISH = 'binary_sensor.template.publish'
BINARY_SENSOR_TEMPLATE_PUBLISH_ACTION_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.use_variable_id(binary_sensor.BinarySensor),
    vol.Required(CONF_STATE): cv.templatable(cv.boolean),
})


@ACTION_REGISTRY.register(CONF_BINARY_SENSOR_TEMPLATE_PUBLISH,
                          BINARY_SENSOR_TEMPLATE_PUBLISH_ACTION_SCHEMA)
def binary_sensor_template_publish_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_binary_sensor_publish_action(template_arg)
    type = BinarySensorPublishAction.template(arg_type)
    action = Pvariable(action_id, rhs, type=type)
    for template_ in templatable(config[CONF_STATE], arg_type, bool_):
        yield None
    add(action.set_state(template_))
    yield action


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)
