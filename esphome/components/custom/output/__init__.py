import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_OUTPUTS, CONF_TYPE
from .. import custom_ns

CustomBinaryOutputConstructor = custom_ns.class_('CustomBinaryOutputConstructor')
CustomFloatOutputConstructor = custom_ns.class_('CustomFloatOutputConstructor')

BINARY_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CustomBinaryOutputConstructor),
    cv.Required(CONF_LAMBDA): cv.returning_lambda,
    cv.Required(CONF_TYPE): 'binary',
    cv.Required(CONF_OUTPUTS):
        cv.ensure_list(output.BINARY_OUTPUT_SCHEMA.extend({
            cv.GenerateID(): cv.declare_id(output.BinaryOutput),
        })),
})

FLOAT_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CustomFloatOutputConstructor),
    cv.Required(CONF_LAMBDA): cv.returning_lambda,
    cv.Required(CONF_TYPE): 'float',
    cv.Required(CONF_OUTPUTS):
        cv.ensure_list(output.FLOAT_OUTPUT_SCHEMA.extend({
            cv.GenerateID(): cv.declare_id(output.FloatOutput),
        })),
})


def validate_custom_output(value):
    if not isinstance(value, dict):
        raise cv.Invalid("Value must be dict")
    if CONF_TYPE not in value:
        raise cv.Invalid("type not specified!")
    type = cv.string_strict(value[CONF_TYPE]).lower()
    value[CONF_TYPE] = type
    if type == 'binary':
        return BINARY_SCHEMA(value)
    if type == 'float':
        return FLOAT_SCHEMA(value)
    raise cv.Invalid("type must either be binary or float, not {}!".format(type))


CONFIG_SCHEMA = validate_custom_output


def to_code(config):
    type = config[CONF_TYPE]
    if type == 'binary':
        ret_type = output.BinaryOutputPtr
        klass = CustomBinaryOutputConstructor
    else:
        ret_type = output.FloatOutputPtr
        klass = CustomFloatOutputConstructor
    template_ = yield cg.process_lambda(config[CONF_LAMBDA], [],
                                        return_type=cg.std_vector.template(ret_type))

    rhs = klass(template_)
    custom = cg.variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_OUTPUTS]):
        out = cg.Pvariable(conf[CONF_ID], custom.get_output(i))
        yield output.register_output(out, conf)
