import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID, CONF_FILE, CONF_DEVICE
from esphome.components import uart

DEPENDENCIES = ['uart']

dfplayer_ns = cg.esphome_ns.namespace('dfplayer')
DFPlayer = dfplayer_ns.class_('DFPlayer', cg.Component)
DFPlayerFinishedPlaybackTrigger = dfplayer_ns.class_('DFPlayerFinishedPlaybackTrigger',
                                                     automation.Trigger.template())
DFPlayerIsPlayingCondition = dfplayer_ns.class_('DFPlayerIsPlayingCondition', automation.Condition)

MULTI_CONF = True
CONF_FOLDER = 'folder'
CONF_LOOP = 'loop'
CONF_VOLUME = 'volume'
CONF_EQ_PRESET = 'eq_preset'
CONF_ON_FINISHED_PLAYBACK = 'on_finished_playback'

EqPreset = dfplayer_ns.enum("EqPreset")
EQ_PRESET = {
    'NORMAL': EqPreset.NORMAL,
    'POP': EqPreset.POP,
    'ROCK': EqPreset.ROCK,
    'JAZZ': EqPreset.JAZZ,
    'CLASSIC': EqPreset.CLASSIC,
    'BASS': EqPreset.BASS,
}
Device = dfplayer_ns.enum("Device")
DEVICE = {
    'USB': Device.USB,
    'TF_CARD': Device.TF_CARD,
}

NextAction = dfplayer_ns.class_('NextAction', automation.Action)
PreviousAction = dfplayer_ns.class_('PreviousAction', automation.Action)
PlayFileAction = dfplayer_ns.class_('PlayFileAction', automation.Action)
PlayFolderAction = dfplayer_ns.class_('PlayFolderAction', automation.Action)
SetVolumeAction = dfplayer_ns.class_('SetVolumeAction', automation.Action)
SetEqAction = dfplayer_ns.class_('SetEqAction', automation.Action)
SleepAction = dfplayer_ns.class_('SleepAction', automation.Action)
ResetAction = dfplayer_ns.class_('ResetAction', automation.Action)
StartAction = dfplayer_ns.class_('StartAction', automation.Action)
PauseAction = dfplayer_ns.class_('PauseAction', automation.Action)
StopAction = dfplayer_ns.class_('StopAction', automation.Action)
RandomAction = dfplayer_ns.class_('RandomAction', automation.Action)
SetDeviceAction = dfplayer_ns.class_('SetDeviceAction', automation.Action)

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(DFPlayer),
    cv.Optional(CONF_ON_FINISHED_PLAYBACK): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DFPlayerFinishedPlaybackTrigger),
    }),
}).extend(uart.UART_DEVICE_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    for conf in config.get(CONF_ON_FINISHED_PLAYBACK, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [], conf)


@automation.register_action('dfplayer.play_next', NextAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayer),
}))
def dfplayer_next_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


@automation.register_action('dfplayer.play_previous', PreviousAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayer),
}))
def dfplayer_previous_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


@automation.register_action('dfplayer.play', PlayFileAction, cv.maybe_simple_value({
    cv.GenerateID(): cv.use_id(DFPlayer),
    cv.Required(CONF_FILE): cv.templatable(cv.int_),
    cv.Optional(CONF_LOOP): cv.templatable(cv.boolean),
}, key=CONF_FILE))
def dfplayer_play_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    template_ = yield cg.templatable(config[CONF_FILE], args, float)
    cg.add(var.set_file(template_))
    if CONF_LOOP in config:
        template_ = yield cg.templatable(config[CONF_LOOP], args, float)
        cg.add(var.set_loop(template_))
    yield var


@automation.register_action('dfplayer.play_folder', PlayFolderAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayer),
    cv.Required(CONF_FOLDER): cv.templatable(cv.int_),
    cv.Optional(CONF_FILE): cv.templatable(cv.int_),
    cv.Optional(CONF_LOOP): cv.templatable(cv.boolean),
}))
def dfplayer_play_folder_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    template_ = yield cg.templatable(config[CONF_FOLDER], args, float)
    cg.add(var.set_folder(template_))
    if CONF_FILE in config:
        template_ = yield cg.templatable(config[CONF_FILE], args, float)
        cg.add(var.set_file(template_))
    if CONF_LOOP in config:
        template_ = yield cg.templatable(config[CONF_LOOP], args, float)
        cg.add(var.set_loop(template_))
    yield var


@automation.register_action('dfplayer.set_device', SetDeviceAction, cv.maybe_simple_value({
    cv.GenerateID(): cv.use_id(DFPlayer),
    cv.Required(CONF_DEVICE): cv.enum(DEVICE, upper=True),
}, key=CONF_DEVICE))
def dfplayer_set_device_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    template_ = yield cg.templatable(config[CONF_DEVICE], args, Device)
    cg.add(var.set_device(template_))
    yield var


@automation.register_action('dfplayer.set_volume', SetVolumeAction, cv.maybe_simple_value({
    cv.GenerateID(): cv.use_id(DFPlayer),
    cv.Required(CONF_VOLUME): cv.templatable(cv.int_),
}, key=CONF_VOLUME))
def dfplayer_set_volume_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    template_ = yield cg.templatable(config[CONF_VOLUME], args, float)
    cg.add(var.set_volume(template_))
    yield var


@automation.register_action('dfplayer.set_eq', SetEqAction, cv.maybe_simple_value({
    cv.GenerateID(): cv.use_id(DFPlayer),
    cv.Required(CONF_EQ_PRESET): cv.templatable(cv.enum(EQ_PRESET, upper=True)),
}, key=CONF_EQ_PRESET))
def dfplayer_set_eq_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    template_ = yield cg.templatable(config[CONF_EQ_PRESET], args, EqPreset)
    cg.add(var.set_eq(template_))
    yield var


@automation.register_action('dfplayer.sleep', SleepAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayer),
}))
def dfplayer_sleep_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


@automation.register_action('dfplayer.reset', ResetAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayer),
}))
def dfplayer_reset_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


@automation.register_action('dfplayer.start', StartAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayer),
}))
def dfplayer_start_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


@automation.register_action('dfplayer.pause', PauseAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayer),
}))
def dfplayer_pause_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


@automation.register_action('dfplayer.stop', StopAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayer),
}))
def dfplayer_stop_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


@automation.register_action('dfplayer.random', RandomAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayer),
}))
def dfplayer_random_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


@automation.register_condition('dfplayer.is_playing', DFPlayerIsPlayingCondition, cv.Schema({
    cv.GenerateID(): cv.use_id(DFPlayer),
}))
def dfplyaer_is_playing_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var
