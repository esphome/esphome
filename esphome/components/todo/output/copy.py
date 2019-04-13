from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_OUTPUTS, CONF_TYPE
BinaryCopyOutput = output.output_ns.class_('BinaryCopyOutput', output.BinaryOutput)
FloatCopyOutput = output.output_ns.class_('FloatCopyOutput', output.FloatOutput)

BINARY_SCHEMA = output.PLATFORM_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_variable_id(BinaryCopyOutput),
    cv.Required(CONF_TYPE): 'binary',
    cv.Required(CONF_OUTPUTS): cv.ensure_list(cv.use_variable_id(output.BinaryOutput)),
})

FLOAT_SCHEMA = output.PLATFORM_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_variable_id(FloatCopyOutput),
    cv.Required(CONF_TYPE): 'float',
    cv.Required(CONF_OUTPUTS): cv.ensure_list(cv.use_variable_id(output.FloatOutput)),
})


def validate_copy_output(value):
    if not isinstance(value, dict):
        raise cv.Invalid("Value must be dict")
    type = cv.string_strict(value.get(CONF_TYPE, 'float')).lower()
    value[CONF_TYPE] = type
    if type == 'binary':
        return BINARY_SCHEMA(value)
    if type == 'float':
        return FLOAT_SCHEMA(value)
    raise cv.Invalid("type must either be binary or float, not {}!".format(type))


PLATFORM_SCHEMA = validate_copy_output


def to_code(config):
    outputs = []
    for out in config[CONF_OUTPUTS]:
        outputs.append((yield get_variable(out)))

    klass = {
        'binary': BinaryCopyOutput,
        'float': FloatCopyOutput,
    }[config[CONF_TYPE]]
    rhs = klass.new(outputs)
    gpio = Pvariable(config[CONF_ID], rhs)

    output.setup_output_platform(gpio, config)
    register_component(gpio, config)


BUILD_FLAGS = '-DUSE_COPY_OUTPUT'
