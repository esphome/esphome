import hashlib

from esphome import config_validation as cv, automation
from esphome import codegen as cg
from esphome.const import CONF_ID, CONF_INITIAL_VALUE, CONF_TYPE, CONF_VALUE, ESP_PLATFORM_ESP32
from esphome.core import coroutine_with_priority

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

CONF_NVS_NAMESPACE = 'nvs_namespace'
CONF_KEY = 'key'

esp32nvs_ns = cg.esphome_ns.namespace('esp32_nvs')
Esp32NvsComponent = esp32nvs_ns.class_('Esp32NvsComponent', cg.Component)
Esp32NvsVarSetAction = esp32nvs_ns.class_('Esp32NvsVarSetAction', automation.Action)
Esp32NvsEraseAction = esp32nvs_ns.class_('Esp32NvsEraseAction', automation.Action)
Esp32NvsSetIdAction = esp32nvs_ns.class_('Esp32NvsSetIdAction', automation.Action)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema({
    #could not figure out how to set a max length of 15 for the ID
    cv.Required(CONF_ID): cv.declare_id(Esp32NvsComponent),
    cv.Required(CONF_TYPE): cv.Any(cv.string_strict, cv.int_, cv.float_),
    cv.Required(CONF_NVS_NAMESPACE): cv.All(cv.string, cv.Length(max=15), cv.Length(min=1)),
    cv.Required(CONF_INITIAL_VALUE): cv.Any(cv.string_strict, cv.int_, cv.float_),
}).extend(cv.COMPONENT_SCHEMA)


# Run with low priority so that namespaces are registered first
@coroutine_with_priority(-100.0)
def to_code(config):
    type_ = cg.RawExpression(config[CONF_TYPE])
    template_args = cg.TemplateArguments(type_)
    res_type = Esp32NvsComponent.template(template_args)

    initial_value = config[CONF_INITIAL_VALUE]

    rhs = Esp32NvsComponent.new(template_args, initial_value)
    glob = cg.Pvariable(config[CONF_ID], rhs, res_type)
    yield cg.register_component(glob, config)

    cg.add(glob.set_nvs_ns_key(config[CONF_NVS_NAMESPACE], config[CONF_ID].id))

@automation.register_action('esp32_nvs.set', Esp32NvsVarSetAction, cv.Schema({
    cv.Required(CONF_ID): cv.use_id(Esp32NvsComponent),
    cv.Required(CONF_VALUE): cv.templatable(cv.string_strict),
}))
def esp32nvs_set_to_code(config, action_id, template_arg, args):
    full_id, paren = yield cg.get_variable_with_full_id(config[CONF_ID])
    template_arg = cg.TemplateArguments(full_id.type, *template_arg)
    var = cg.new_Pvariable(action_id, template_arg, paren)
    templ = yield cg.templatable(config[CONF_VALUE], args, None, to_exp=cg.RawExpression)
    cg.add(var.set_value(templ))
    yield var

@automation.register_action('esp32_nvs.erase', Esp32NvsEraseAction, cv.Schema({
    cv.Required(CONF_ID): cv.use_id(Esp32NvsComponent)
}))
def esp32nvs_erase_to_code(config, action_id, template_arg, args):
    full_id, paren = yield cg.get_variable_with_full_id(config[CONF_ID])
    template_arg = cg.TemplateArguments(full_id.type, *template_arg)
    var = cg.new_Pvariable(action_id, template_arg, paren)
    yield var

@automation.register_action('esp32_nvs.set_id', Esp32NvsSetIdAction, cv.Schema({
    cv.Required(CONF_ID): cv.use_id(Esp32NvsComponent),
    cv.Required(CONF_NVS_NAMESPACE): cv.templatable(cv.string),
    cv.Required(CONF_KEY): cv.templatable(cv.string),
    cv.Required(CONF_VALUE): cv.templatable(cv.string_strict),
}))
def esp32nvs_set_id_to_code(config, action_id, template_arg, args):
    full_id, paren = yield cg.get_variable_with_full_id(config[CONF_ID])
    template_arg = cg.TemplateArguments(full_id.type, *template_arg)
    var = cg.new_Pvariable(action_id, template_arg, paren)
    templ = yield cg.templatable(config[CONF_VALUE], args, None, to_exp=cg.RawExpression)
    cg.add(var.set_value(templ))
    yield var
