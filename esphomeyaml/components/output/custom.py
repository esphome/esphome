import voluptuous as vol

from esphomeyaml.components import binary_sensor, output
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_LAMBDA, CONF_OUTPUTS, CONF_TYPE
from esphomeyaml.cpp_generator import process_lambda, variable
from esphomeyaml.cpp_types import std_vector

CustomBinaryOutputConstructor = binary_sensor.binary_sensor_ns.class_(
    'CustomBinaryOutputConstructor')
CustomFloatOutputConstructor = binary_sensor.binary_sensor_ns.class_(
    'CustomFloatOutputConstructor')

BINARY_SCHEMA = output.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CustomBinaryOutputConstructor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Required(CONF_OUTPUTS):
        vol.All(cv.ensure_list, [output.BINARY_OUTPUT_SCHEMA.extend({
            cv.GenerateID(): cv.declare_variable_id(output.BinaryOutput),
        })]),
})

FLOAT_SCHEMA = output.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CustomFloatOutputConstructor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Required(CONF_OUTPUTS):
        vol.All(cv.ensure_list, [output.FLOAT_OUTPUT_PLATFORM_SCHEMA.extend({
            cv.GenerateID(): cv.declare_variable_id(output.FloatOutput),
        })]),
})


def validate_custom_output(value):
    if not isinstance(value, dict):
        raise vol.Invalid("Value must be dict")
    if CONF_TYPE not in value:
        raise vol.Invalid("type not specified!")
    type = cv.string_strict(value[CONF_TYPE]).lower()
    value[CONF_TYPE] = type
    if type == 'binary':
        return BINARY_SCHEMA(value)
    elif type == 'float':
        return FLOAT_SCHEMA(value)
    raise vol.Invalid("type must either be binary or float, not {}!".format(type))


PLATFORM_SCHEMA = validate_custom_output


def to_code(config):
    type = config[CONF_TYPE]
    if type == 'binary':
        ret_type = output.BinaryOutputPtr
        klass = CustomBinaryOutputConstructor
    else:
        ret_type = output.FloatOutputPtr
        klass = CustomFloatOutputConstructor
    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=std_vector.template(ret_type)):
        yield

    rhs = klass(template_)
    custom = variable(config[CONF_ID], rhs)
    for i, sens in enumerate(config[CONF_OUTPUTS]):
        output.register_output(custom.get_output(i), sens)


BUILD_FLAGS = '-DUSE_CUSTOM_OUTPUT'


def to_hass_config(data, config):
    return [binary_sensor.core_to_hass_config(data, sens) for sens in config[CONF_OUTPUTS]]
