import voluptuous as vol

from esphomeyaml import config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_INITIAL_VALUE, CONF_RESTORE_VALUE, CONF_TYPE
from esphomeyaml.helpers import App, Component, Pvariable, RawExpression, TemplateArguments, add, \
    esphomelib_ns, setup_component

GlobalVariableComponent = esphomelib_ns.class_('GlobalVariableComponent', Component)

GLOBAL_VAR_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(GlobalVariableComponent),
    vol.Required(CONF_TYPE): cv.string_strict,
    vol.Optional(CONF_INITIAL_VALUE): cv.string_strict,
    vol.Optional(CONF_RESTORE_VALUE): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA.schema)

CONFIG_SCHEMA = vol.All(cv.ensure_list, [GLOBAL_VAR_SCHEMA])


def to_code(config):
    for conf in config:
        type_ = RawExpression(conf[CONF_TYPE])
        template_args = TemplateArguments(type_)
        res_type = GlobalVariableComponent.template(template_args)
        initial_value = None
        if CONF_INITIAL_VALUE in conf:
            initial_value = RawExpression(conf[CONF_INITIAL_VALUE])
        rhs = App.Pmake_global_variable(template_args, initial_value)
        glob = Pvariable(conf[CONF_ID], rhs, type=res_type)

        if conf.get(CONF_RESTORE_VALUE, False):
            hash_ = hash(conf[CONF_ID].id) % 2**32
            add(glob.set_restore_value(hash_))

        setup_component(glob, conf)
