import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import cover, fan
from esphomeyaml.const import CONF_ABOVE, CONF_ACTION_ID, CONF_AND, CONF_AUTOMATION_ID, \
    CONF_BELOW, \
    CONF_BLUE, CONF_BRIGHTNESS, CONF_CONDITION_ID, CONF_DELAY, CONF_EFFECT, CONF_FLASH_LENGTH, \
    CONF_GREEN, CONF_ID, CONF_IF, CONF_LAMBDA, CONF_OR, CONF_OSCILLATING, CONF_PAYLOAD, CONF_QOS, \
    CONF_RANGE, CONF_RED, CONF_RETAIN, CONF_SPEED, CONF_THEN, CONF_TOPIC, CONF_TRANSITION_LENGTH, \
    CONF_TRIGGER_ID, CONF_WHITE
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, ArrayInitializer, Pvariable, TemplateArguments, add, \
    bool_, esphomelib_ns, float_, get_variable, process_lambda, std_string, templatable, uint32, \
    uint8, add_job

CONF_MQTT_PUBLISH = 'mqtt.publish'
CONF_LIGHT_TOGGLE = 'light.toggle'
CONF_LIGHT_TURN_OFF = 'light.turn_off'
CONF_LIGHT_TURN_ON = 'light.turn_on'
CONF_SWITCH_TOGGLE = 'switch.toggle'
CONF_SWITCH_TURN_OFF = 'switch.turn_off'
CONF_SWITCH_TURN_ON = 'switch.turn_on'
CONF_COVER_OPEN = 'cover.open'
CONF_COVER_CLOSE = 'cover.close'
CONF_COVER_STOP = 'cover.stop'
CONF_FAN_TOGGLE = 'fan.toggle'
CONF_FAN_TURN_OFF = 'fan.turn_off'
CONF_FAN_TURN_ON = 'fan.turn_on'

ACTION_KEYS = [CONF_DELAY, CONF_MQTT_PUBLISH, CONF_LIGHT_TOGGLE, CONF_LIGHT_TURN_OFF,
               CONF_LIGHT_TURN_ON, CONF_SWITCH_TOGGLE, CONF_SWITCH_TURN_OFF, CONF_SWITCH_TURN_ON,
               CONF_LAMBDA, CONF_COVER_OPEN, CONF_COVER_CLOSE, CONF_COVER_STOP, CONF_FAN_TOGGLE,
               CONF_FAN_TURN_OFF, CONF_FAN_TURN_ON]

ACTIONS_SCHEMA = vol.All(cv.ensure_list, [vol.All({
    cv.GenerateID(CONF_ACTION_ID): cv.declare_variable_id(None),
    vol.Optional(CONF_DELAY): cv.templatable(cv.positive_time_period_milliseconds),
    vol.Optional(CONF_MQTT_PUBLISH): vol.Schema({
        vol.Required(CONF_TOPIC): cv.templatable(cv.publish_topic),
        vol.Required(CONF_PAYLOAD): cv.templatable(cv.mqtt_payload),
        vol.Optional(CONF_QOS): cv.templatable(cv.mqtt_qos),
        vol.Optional(CONF_RETAIN): cv.templatable(cv.boolean),
    }),
    vol.Optional(CONF_LIGHT_TOGGLE): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
        vol.Optional(CONF_TRANSITION_LENGTH): cv.templatable(cv.positive_time_period_milliseconds),
    }),
    vol.Optional(CONF_LIGHT_TURN_OFF): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
        vol.Optional(CONF_TRANSITION_LENGTH): cv.templatable(cv.positive_time_period_milliseconds),
    }),
    vol.Optional(CONF_LIGHT_TURN_ON): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
        vol.Exclusive(CONF_TRANSITION_LENGTH, 'transformer'):
            cv.templatable(cv.positive_time_period_milliseconds),
        vol.Exclusive(CONF_FLASH_LENGTH, 'transformer'):
            cv.templatable(cv.positive_time_period_milliseconds),
        vol.Optional(CONF_BRIGHTNESS): cv.templatable(cv.percentage),
        vol.Optional(CONF_RED): cv.templatable(cv.percentage),
        vol.Optional(CONF_GREEN): cv.templatable(cv.percentage),
        vol.Optional(CONF_BLUE): cv.templatable(cv.percentage),
        vol.Optional(CONF_WHITE): cv.templatable(cv.percentage),
        vol.Optional(CONF_EFFECT): cv.templatable(cv.string),
    }),
    vol.Optional(CONF_SWITCH_TOGGLE): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_SWITCH_TURN_OFF): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_SWITCH_TURN_ON): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_COVER_OPEN): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_COVER_CLOSE): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_COVER_STOP): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_COVER_OPEN): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_COVER_CLOSE): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_COVER_STOP): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_FAN_TOGGLE): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_FAN_TURN_OFF): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
    }),
    vol.Optional(CONF_FAN_TURN_ON): vol.Schema({
        vol.Required(CONF_ID): cv.use_variable_id(None),
        vol.Optional(CONF_OSCILLATING): cv.templatable(cv.boolean),
        vol.Optional(CONF_SPEED): cv.templatable(fan.validate_fan_speed),
    }),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
}, cv.has_exactly_one_key(*ACTION_KEYS))])

# pylint: disable=invalid-name
DelayAction = esphomelib_ns.DelayAction
LambdaAction = esphomelib_ns.LambdaAction
Automation = esphomelib_ns.Automation


def validate_recursive_condition(value):
    return CONDITIONS_SCHEMA(value)


CONDITION_KEYS = [CONF_AND, CONF_OR, CONF_RANGE, CONF_LAMBDA]

CONDITIONS_SCHEMA = vol.All(cv.ensure_list, [vol.All({
    cv.GenerateID(CONF_CONDITION_ID): cv.declare_variable_id(None),
    vol.Optional(CONF_AND): validate_recursive_condition,
    vol.Optional(CONF_OR): validate_recursive_condition,
    vol.Optional(CONF_RANGE): vol.All(vol.Schema({
        vol.Optional(CONF_ABOVE): vol.Coerce(float),
        vol.Optional(CONF_BELOW): vol.Coerce(float),
    }), cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW)),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
}), cv.has_exactly_one_key(*CONDITION_KEYS)])

# pylint: disable=invalid-name
AndCondition = esphomelib_ns.AndCondition
OrCondition = esphomelib_ns.OrCondition
RangeCondition = esphomelib_ns.RangeCondition
LambdaCondition = esphomelib_ns.LambdaCondition

AUTOMATION_SCHEMA = vol.Schema({
    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(None),
    cv.GenerateID(CONF_AUTOMATION_ID): cv.declare_variable_id(None),
    vol.Optional(CONF_IF): CONDITIONS_SCHEMA,
    vol.Required(CONF_THEN): ACTIONS_SCHEMA,
})


def build_condition(config, arg_type):
    template_arg = TemplateArguments(arg_type)
    if CONF_AND in config:
        yield AndCondition.new(template_arg, build_conditions(config[CONF_AND], template_arg))
    elif CONF_OR in config:
        yield OrCondition.new(template_arg, build_conditions(config[CONF_OR], template_arg))
    elif CONF_LAMBDA in config:
        lambda_ = None
        for lambda_ in process_lambda(config[CONF_LAMBDA], [(arg_type, 'x')]):
            yield
        yield LambdaCondition.new(template_arg, lambda_)
    elif CONF_RANGE in config:
        conf = config[CONF_RANGE]
        rhs = RangeCondition.new(template_arg)
        type = RangeCondition.template(template_arg)
        condition = Pvariable(config[CONF_CONDITION_ID], rhs, type=type)
        if CONF_ABOVE in conf:
            template_ = None
            for template_ in templatable(conf[CONF_ABOVE], arg_type, float_):
                yield
            condition.set_min(template_)
        if CONF_BELOW in conf:
            template_ = None
            for template_ in templatable(conf[CONF_BELOW], arg_type, float_):
                yield
            condition.set_max(template_)
        yield condition
    else:
        raise ESPHomeYAMLError(u"Unsupported condition {}".format(config))


def build_conditions(config, arg_type):
    conditions = []
    for conf in config:
        condition = None
        for condition in build_condition(conf, arg_type):
            yield None
        conditions.append(condition)
    yield ArrayInitializer(*conditions)


def build_action(config, arg_type):
    from esphomeyaml.components import light, mqtt, switch

    template_arg = TemplateArguments(arg_type)
    # Keep pylint from freaking out
    var = None
    if CONF_DELAY in config:
        rhs = App.register_component(DelayAction.new(template_arg))
        type = DelayAction.template(template_arg)
        action = Pvariable(config[CONF_ACTION_ID], rhs, type=type)
        template_ = None
        for template_ in templatable(config[CONF_DELAY], arg_type, uint32):
            yield
        add(action.set_delay(template_))
        yield action
    elif CONF_LAMBDA in config:
        lambda_ = None
        for lambda_ in process_lambda(config[CONF_LAMBDA], [(arg_type, 'x')]):
            yield None
        rhs = LambdaAction.new(template_arg, lambda_)
        type = LambdaAction.template(template_arg)
        yield Pvariable(config[CONF_ACTION_ID], rhs, type=type)
    elif CONF_MQTT_PUBLISH in config:
        conf = config[CONF_MQTT_PUBLISH]
        rhs = App.Pget_mqtt_client().Pmake_publish_action(template_arg)
        type = mqtt.MQTTPublishAction.template(template_arg)
        action = Pvariable(config[CONF_ACTION_ID], rhs, type=type)
        template_ = None
        for template_ in templatable(conf[CONF_TOPIC], arg_type, std_string):
            yield None
        add(action.set_topic(template_))

        template_ = None
        for template_ in templatable(conf[CONF_PAYLOAD], arg_type, std_string):
            yield None
        add(action.set_payload(template_))
        if CONF_QOS in conf:
            template_ = None
            for template_ in templatable(conf[CONF_QOS], arg_type, uint8):
                yield
            add(action.set_qos(template_))
        if CONF_RETAIN in conf:
            template_ = None
            for template_ in templatable(conf[CONF_RETAIN], arg_type, bool_):
                yield None
            add(action.set_retain(template_))
        yield action
    elif CONF_LIGHT_TOGGLE in config:
        conf = config[CONF_LIGHT_TOGGLE]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_toggle_action(template_arg)
        type = light.ToggleAction.template(template_arg)
        action = Pvariable(config[CONF_ACTION_ID], rhs, type=type)
        if CONF_TRANSITION_LENGTH in conf:
            template_ = None
            for template_ in templatable(conf[CONF_TRANSITION_LENGTH], arg_type, uint32):
                yield None
            add(action.set_transition_length(template_))
        yield action
    elif CONF_LIGHT_TURN_OFF in config:
        conf = config[CONF_LIGHT_TURN_OFF]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_turn_off_action(template_arg)
        type = light.TurnOffAction.template(template_arg)
        action = Pvariable(config[CONF_ACTION_ID], rhs, type=type)
        if CONF_TRANSITION_LENGTH in conf:
            template_ = None
            for template_ in templatable(conf[CONF_TRANSITION_LENGTH], arg_type, uint32):
                yield None
            add(action.set_transition_length(template_))
        yield action
    elif CONF_LIGHT_TURN_ON in config:
        conf = config[CONF_LIGHT_TURN_ON]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_turn_on_action(template_arg)
        type = light.TurnOnAction.template(template_arg)
        action = Pvariable(config[CONF_ACTION_ID], rhs, type=type)
        if CONF_TRANSITION_LENGTH in conf:
            template_ = None
            for template_ in templatable(conf[CONF_TRANSITION_LENGTH], arg_type, uint32):
                yield None
            add(action.set_transition_length(template_))
        if CONF_FLASH_LENGTH in conf:
            template_ = None
            for template_ in templatable(conf[CONF_FLASH_LENGTH], arg_type, uint32):
                yield None
            add(action.set_flash_length(template_))
        if CONF_BRIGHTNESS in conf:
            template_ = None
            for template_ in templatable(conf[CONF_BRIGHTNESS], arg_type, float_):
                yield None
            add(action.set_brightness(template_))
        if CONF_RED in conf:
            template_ = None
            for template_ in templatable(conf[CONF_RED], arg_type, float_):
                yield None
            add(action.set_red(template_))
        if CONF_GREEN in conf:
            template_ = None
            for template_ in templatable(conf[CONF_GREEN], arg_type, float_):
                yield None
            add(action.set_green(template_))
        if CONF_BLUE in conf:
            template_ = None
            for template_ in templatable(conf[CONF_BLUE], arg_type, float_):
                yield None
            add(action.set_blue(template_))
        if CONF_WHITE in conf:
            template_ = None
            for template_ in templatable(conf[CONF_WHITE], arg_type, float_):
                yield None
            add(action.set_white(template_))
        if CONF_EFFECT in conf:
            template_ = None
            for template_ in templatable(conf[CONF_EFFECT], arg_type, std_string):
                yield None
            add(action.set_effect(template_))
        yield action
    elif CONF_SWITCH_TOGGLE in config:
        conf = config[CONF_SWITCH_TOGGLE]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_toggle_action(template_arg)
        type = switch.ToggleAction.template(arg_type)
        yield Pvariable(config[CONF_ACTION_ID], rhs, type=type)
    elif CONF_SWITCH_TURN_OFF in config:
        conf = config[CONF_SWITCH_TURN_OFF]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_turn_off_action(template_arg)
        type = switch.TurnOffAction.template(arg_type)
        yield Pvariable(config[CONF_ACTION_ID], rhs, type=type)
    elif CONF_SWITCH_TURN_ON in config:
        conf = config[CONF_SWITCH_TURN_ON]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_turn_on_action(template_arg)
        type = switch.TurnOnAction.template(arg_type)
        yield Pvariable(config[CONF_ACTION_ID], rhs, type=type)
    elif CONF_COVER_OPEN in config:
        conf = config[CONF_COVER_OPEN]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_open_action(template_arg)
        type = cover.OpenAction.template(arg_type)
        yield Pvariable(config[CONF_ACTION_ID], rhs, type=type)
    elif CONF_COVER_CLOSE in config:
        conf = config[CONF_COVER_CLOSE]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_close_action(template_arg)
        type = cover.CloseAction.template(arg_type)
        yield Pvariable(config[CONF_ACTION_ID], rhs, type=type)
    elif CONF_COVER_STOP in config:
        conf = config[CONF_COVER_STOP]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_stop_action(template_arg)
        type = cover.StopAction.template(arg_type)
        yield Pvariable(config[CONF_ACTION_ID], rhs, type=type)
    elif CONF_FAN_TOGGLE in config:
        conf = config[CONF_FAN_TOGGLE]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_toggle_action(template_arg)
        type = fan.ToggleAction.template(arg_type)
        yield Pvariable(config[CONF_ACTION_ID], rhs, type=type)
    elif CONF_FAN_TURN_OFF in config:
        conf = config[CONF_FAN_TURN_OFF]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_turn_off_action(template_arg)
        type = fan.TurnOffAction.template(arg_type)
        yield Pvariable(config[CONF_ACTION_ID], rhs, type=type)
    elif CONF_FAN_TURN_ON in config:
        conf = config[CONF_FAN_TURN_ON]
        for var in get_variable(conf[CONF_ID]):
            yield None
        rhs = var.make_turn_on_action(template_arg)
        type = fan.TurnOnAction.template(arg_type)
        action = Pvariable(config[CONF_ACTION_ID], rhs, type=type)
        if CONF_OSCILLATING in config:
            template_ = None
            for template_ in templatable(conf[CONF_OSCILLATING], arg_type, bool_):
                yield None
            add(action.set_oscillating(template_))
        if CONF_SPEED in config:
            template_ = None
            for template_ in templatable(conf[CONF_SPEED], arg_type, fan.FanSpeed):
                yield None
            add(action.set_speed(template_))
        yield action
    else:
        raise ESPHomeYAMLError(u"Unsupported action {}".format(config))


def build_actions(config, arg_type):
    actions = []
    for conf in config:
        action = None
        for action in build_action(conf, arg_type):
            yield None
        actions.append(action)
    yield ArrayInitializer(*actions, multiline=False)


def build_automation_(trigger, arg_type, config):
    rhs = App.make_automation(trigger)
    type = Automation.template(arg_type)
    obj = Pvariable(config[CONF_AUTOMATION_ID], rhs, type=type)
    if CONF_IF in config:
        conditions = None
        for conditions in build_conditions(config[CONF_IF], arg_type):
            yield
        add(obj.add_conditions(conditions))
    actions = None
    for actions in build_actions(config[CONF_THEN], arg_type):
        yield
    add(obj.add_actions(actions))


def build_automation(trigger, arg_type, config):
    add_job(build_automation_, trigger, arg_type, config)
