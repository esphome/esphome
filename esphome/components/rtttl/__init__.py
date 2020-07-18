import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components.output import FloatOutput
from esphome.const import CONF_ID, CONF_OUTPUT

rtttl_ns = cg.esphome_ns.namespace('rtttl')
Rtttl = rtttl_ns .class_('Rtttl', cg.Component)
RtttlPlayAction = rtttl_ns.class_('RtttlPlayAction', automation.Action)

CONF_RTTTL = 'rtttl'

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.declare_id(Rtttl),
    cv.Required(CONF_OUTPUT): cv.use_id(FloatOutput),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    out = yield cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))


@automation.register_action('rtttl.play', RtttlPlayAction, cv.Schema({
    cv.GenerateID(CONF_ID): cv.use_id(Rtttl),
    cv.Required(CONF_RTTTL): cv.templatable(cv.string)
}))
def rtttl_play_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_RTTTL], args, cg.std_string)
    cg.add(var.set_value(template_))
    yield var
