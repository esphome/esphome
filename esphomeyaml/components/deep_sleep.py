import voluptuous as vol

from esphomeyaml import config_validation as cv, pins
from esphomeyaml.automation import maybe_simple_id, ACTION_REGISTRY
from esphomeyaml.const import CONF_ID, CONF_NUMBER, CONF_RUN_CYCLES, CONF_RUN_DURATION, \
    CONF_SLEEP_DURATION, CONF_WAKEUP_PIN
from esphomeyaml.helpers import App, Pvariable, add, gpio_input_pin_expression, esphomelib_ns, \
    TemplateArguments, get_variable


def validate_pin_number(value):
    valid_pins = [0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 39]
    if value[CONF_NUMBER] not in valid_pins:
        raise vol.Invalid(u"Only pins {} support wakeup"
                          u"".format(', '.join(str(x) for x in valid_pins)))
    return value


DeepSleepComponent = esphomelib_ns.DeepSleepComponent
EnterDeepSleepAction = esphomelib_ns.EnterDeepSleepAction
PreventDeepSleepAction = esphomelib_ns.PreventDeepSleepAction

WAKEUP_PIN_MODES = {
    'IGNORE': esphomelib_ns.WAKEUP_PIN_MODE_IGNORE,
    'KEEP_AWAKE': esphomelib_ns.WAKEUP_PIN_MODE_KEEP_AWAKE,
    'INVERT_WAKEUP': esphomelib_ns.WAKEUP_PIN_MODE_INVERT_WAKEUP,
}

CONF_WAKEUP_PIN_MODE = 'wakeup_pin_mode'

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(DeepSleepComponent),
    vol.Optional(CONF_SLEEP_DURATION): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_WAKEUP_PIN): vol.All(cv.only_on_esp32, pins.internal_gpio_input_pin_schema,
                                           validate_pin_number),
    vol.Optional(CONF_WAKEUP_PIN_MODE): vol.All(cv.only_on_esp32, vol.Upper,
                                                cv.one_of(*WAKEUP_PIN_MODES)),
    vol.Optional(CONF_RUN_CYCLES): cv.positive_int,
    vol.Optional(CONF_RUN_DURATION): cv.positive_time_period_milliseconds,
})


def to_code(config):
    rhs = App.make_deep_sleep_component()
    deep_sleep = Pvariable(config[CONF_ID], rhs)
    if CONF_SLEEP_DURATION in config:
        add(deep_sleep.set_sleep_duration(config[CONF_SLEEP_DURATION]))
    if CONF_WAKEUP_PIN in config:
        pin = None
        for pin in gpio_input_pin_expression(config[CONF_WAKEUP_PIN]):
            yield
        add(deep_sleep.set_wakeup_pin(pin))
    if CONF_WAKEUP_PIN_MODE in config:
        add(deep_sleep.set_wakeup_pin_mode(WAKEUP_PIN_MODES[config[CONF_WAKEUP_PIN_MODE]]))
    if CONF_RUN_CYCLES in config:
        add(deep_sleep.set_run_cycles(config[CONF_RUN_CYCLES]))
    if CONF_RUN_DURATION in config:
        add(deep_sleep.set_run_duration(config[CONF_RUN_DURATION]))


BUILD_FLAGS = '-DUSE_DEEP_SLEEP'


CONF_DEEP_SLEEP_ENTER = 'deep_sleep.enter'
DEEP_SLEEP_ENTER_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(DeepSleepComponent),
})


@ACTION_REGISTRY.register(CONF_DEEP_SLEEP_ENTER, DEEP_SLEEP_ENTER_ACTION_SCHEMA)
def deep_sleep_enter_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_enter_deep_sleep_action(template_arg)
    type = EnterDeepSleepAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)


CONF_DEEP_SLEEP_PREVENT = 'deep_sleep.prevent'
DEEP_SLEEP_PREVENT_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(DeepSleepComponent),
})


@ACTION_REGISTRY.register(CONF_DEEP_SLEEP_PREVENT, DEEP_SLEEP_PREVENT_ACTION_SCHEMA)
def deep_sleep_prevent_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_prevent_deep_sleep_action(template_arg)
    type = PreventDeepSleepAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)
