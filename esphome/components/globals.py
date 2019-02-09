import voluptuous as vol

from esphome import config_validation as cv
from esphome.const import CONF_ID, CONF_INITIAL_VALUE, CONF_RESTORE_VALUE, CONF_TYPE
from esphome.cpp_generator import Pvariable, RawExpression, TemplateArguments, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component, esphome_ns

GlobalVariableComponent = esphome_ns.class_('GlobalVariableComponent', Component)

MULTI_CONF = True

CONFIG_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(GlobalVariableComponent),
    vol.Required(CONF_TYPE): cv.string_strict,
    vol.Optional(CONF_INITIAL_VALUE): cv.string_strict,
    vol.Optional(CONF_RESTORE_VALUE): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    type_ = RawExpression(config[CONF_TYPE])
    template_args = TemplateArguments(type_)
    res_type = GlobalVariableComponent.template(template_args)
    initial_value = None
    if CONF_INITIAL_VALUE in config:
        initial_value = RawExpression(config[CONF_INITIAL_VALUE])
    rhs = App.Pmake_global_variable(template_args, initial_value)
    glob = Pvariable(config[CONF_ID], rhs, type=res_type)

    if config.get(CONF_RESTORE_VALUE, False):
        hash_ = hash(config[CONF_ID].id) % 2**32
        add(glob.set_restore_value(hash_))

    setup_component(glob, config)
