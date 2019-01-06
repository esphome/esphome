import voluptuous as vol

from esphomeyaml.components import text_sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_LAMBDA, CONF_TEXT_SENSORS
from esphomeyaml.cpp_generator import process_lambda, variable
from esphomeyaml.cpp_types import std_vector

CustomTextSensorConstructor = text_sensor.text_sensor_ns.class_('CustomTextSensorConstructor')

PLATFORM_SCHEMA = text_sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CustomTextSensorConstructor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Required(CONF_TEXT_SENSORS):
        cv.ensure_list(text_sensor.TEXT_SENSOR_SCHEMA.extend({
            cv.GenerateID(): cv.declare_variable_id(text_sensor.TextSensor),
        })),
})


def to_code(config):
    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=std_vector.template(text_sensor.TextSensorPtr)):
        yield

    rhs = CustomTextSensorConstructor(template_)
    custom = variable(config[CONF_ID], rhs)
    for i, sens in enumerate(config[CONF_TEXT_SENSORS]):
        text_sensor.register_text_sensor(custom.get_text_sensor(i), sens)


BUILD_FLAGS = '-DUSE_CUSTOM_TEXT_SENSOR'


def to_hass_config(data, config):
    return [text_sensor.core_to_hass_config(data, sens) for sens in config[CONF_TEXT_SENSORS]]
