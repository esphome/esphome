import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID
from esphome.components import uart
from esphome.automation import maybe_simple_id

DEPENDENCIES = ['uart']

dfplayer_ns = cg.esphome_ns.namespace('dfplayer')
DFPlayerComponent = dfplayer_ns.class_('DFPlayerComponent', cg.Component)

MULTI_CONF = True
CONF_TRACK_NUMBER = 'track'

PlayTrackAction = dfplayer_ns.class_('PlayTrackAction', automation.Action)

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(DFPlayerComponent)
}).extend(cv.polling_component_schema('5s')).extend(uart.UART_DEVICE_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)


@automation.register_action('dfplayer.play_track', PlayTrackAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayerComponent),
    cv.Required(CONF_TRACK_NUMBER): cv.templatable(cv.int_),
}))
def dfplayer_play_track_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_TRACK_NUMBER], args, float)
    cg.add(var.set_track(template_))
    yield var
