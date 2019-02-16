import voluptuous as vol

from esphome.automation import ACTION_REGISTRY
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_STATE
from esphome.cpp_generator import Pvariable, add, get_variable, process_lambda, templatable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import Action, App, Component, bool_, optional

TemplateBinarySensor = binary_sensor.binary_sensor_ns.class_('TemplateBinarySensor',
                                                             binary_sensor.BinarySensor,
                                                             Component)
BinarySensorPublishAction = binary_sensor.binary_sensor_ns.class_('BinarySensorPublishAction',
                                                                  Action)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateBinarySensor),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_template_binary_sensor(config[CONF_NAME])
    var = Pvariable(config[CONF_ID], rhs)
    binary_sensor.setup_binary_sensor(var, config)
    setup_component(var, config)

    if CONF_LAMBDA in config:
        for template_ in process_lambda(config[CONF_LAMBDA], [],
                                        return_type=optional.template(bool_)):
            yield
        add(var.set_template(template_))


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
