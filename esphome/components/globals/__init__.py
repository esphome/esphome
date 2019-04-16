import hashlib

from esphome import config_validation as cv
from esphome import codegen as cg
from esphome.const import CONF_ID, CONF_INITIAL_VALUE, CONF_RESTORE_VALUE, CONF_TYPE
from esphome.py_compat import IS_PY3

globals_ns = cg.esphome_ns.namespace('globals')
GlobalsComponent = globals_ns.class_('GlobalsComponent', cg.Component)

MULTI_CONF = True

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_variable_id(GlobalsComponent),
    cv.Required(CONF_TYPE): cv.string_strict,
    cv.Optional(CONF_INITIAL_VALUE): cv.string_strict,
    cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    type_ = cg.RawExpression(config[CONF_TYPE])
    template_args = cg.TemplateArguments(type_)
    res_type = GlobalsComponent.template(template_args)

    initial_value = None
    if CONF_INITIAL_VALUE in config:
        initial_value = cg.RawExpression(config[CONF_INITIAL_VALUE])

    rhs = GlobalsComponent.new(template_args, initial_value)
    glob = cg.Pvariable(config[CONF_ID], rhs, type=res_type)
    yield cg.register_component(glob, config)

    if config.get(CONF_RESTORE_VALUE, False):
        value = config[CONF_ID].id
        if IS_PY3 and isinstance(value, str):
            value = value.encode()
        hash_ = int(hashlib.md5(value).hexdigest()[:8], 16)
        cg.add(glob.set_restore_value(hash_))
