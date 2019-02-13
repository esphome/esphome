import voluptuous as vol

from esphome import config_validation as cv, pins
from esphome.automation import ACTION_REGISTRY, maybe_simple_id
from esphome.const import CONF_ID, CONF_MODE, CONF_NUMBER, CONF_PINS, CONF_RUN_CYCLES, \
    CONF_RUN_DURATION, CONF_SLEEP_DURATION, CONF_WAKEUP_PIN
from esphome.cpp_generator import Pvariable, StructInitializer, add, get_variable
from esphome.cpp_helpers import gpio_input_pin_expression, setup_component
from esphome.cpp_types import Action, App, Component, esphome_ns, global_ns


def validate_pin_number(value):
    valid_pins = [0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 39]
    if value[CONF_NUMBER] not in valid_pins:
        raise vol.Invalid(u"Only pins {} support wakeup"
                          u"".format(', '.join(str(x) for x in valid_pins)))
    return value


DeepSleepComponent = esphome_ns.class_('DeepSleepComponent', Component)
EnterDeepSleepAction = esphome_ns.class_('EnterDeepSleepAction', Action)
PreventDeepSleepAction = esphome_ns.class_('PreventDeepSleepAction', Action)

WakeupPinMode = esphome_ns.enum('WakeupPinMode')
WAKEUP_PIN_MODES = {
    'IGNORE': WakeupPinMode.WAKEUP_PIN_MODE_IGNORE,
    'KEEP_AWAKE': WakeupPinMode.WAKEUP_PIN_MODE_KEEP_AWAKE,
    'INVERT_WAKEUP': WakeupPinMode.WAKEUP_PIN_MODE_INVERT_WAKEUP,
}

esp_sleep_ext1_wakeup_mode_t = global_ns.enum('esp_sleep_ext1_wakeup_mode_t')
Ext1Wakeup = esphome_ns.struct('Ext1Wakeup')
EXT1_WAKEUP_MODES = {
    'ALL_LOW': esp_sleep_ext1_wakeup_mode_t.ESP_EXT1_WAKEUP_ALL_LOW,
    'ANY_HIGH': esp_sleep_ext1_wakeup_mode_t.ESP_EXT1_WAKEUP_ANY_HIGH,
}

CONF_WAKEUP_PIN_MODE = 'wakeup_pin_mode'
CONF_ESP32_EXT1_WAKEUP = 'esp32_ext1_wakeup'

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(DeepSleepComponent),
    vol.Optional(CONF_SLEEP_DURATION): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_WAKEUP_PIN): vol.All(cv.only_on_esp32, pins.internal_gpio_input_pin_schema,
                                           validate_pin_number),
    vol.Optional(CONF_WAKEUP_PIN_MODE): vol.All(cv.only_on_esp32,
                                                cv.one_of(*WAKEUP_PIN_MODES), upper=True),
    vol.Optional(CONF_ESP32_EXT1_WAKEUP): vol.All(cv.only_on_esp32, vol.Schema({
        vol.Required(CONF_PINS): cv.ensure_list(pins.shorthand_input_pin, validate_pin_number),
        vol.Required(CONF_MODE): cv.one_of(*EXT1_WAKEUP_MODES, upper=True),
    })),
    vol.Optional(CONF_RUN_DURATION): cv.positive_time_period_milliseconds,

    vol.Optional(CONF_RUN_CYCLES): cv.invalid("The run_cycles option has been removed in 1.11.0 as "
                                              "it was essentially the same as a run_duration of 0s."
                                              "Please use run_duration now.")
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_deep_sleep_component()
    deep_sleep = Pvariable(config[CONF_ID], rhs)
    if CONF_SLEEP_DURATION in config:
        add(deep_sleep.set_sleep_duration(config[CONF_SLEEP_DURATION]))
    if CONF_WAKEUP_PIN in config:
        for pin in gpio_input_pin_expression(config[CONF_WAKEUP_PIN]):
            yield
        add(deep_sleep.set_wakeup_pin(pin))
    if CONF_WAKEUP_PIN_MODE in config:
        add(deep_sleep.set_wakeup_pin_mode(WAKEUP_PIN_MODES[config[CONF_WAKEUP_PIN_MODE]]))
    if CONF_RUN_DURATION in config:
        add(deep_sleep.set_run_duration(config[CONF_RUN_DURATION]))

    if CONF_ESP32_EXT1_WAKEUP in config:
        conf = config[CONF_ESP32_EXT1_WAKEUP]
        mask = 0
        for pin in conf[CONF_PINS]:
            mask |= 1 << pin[CONF_NUMBER]
        struct = StructInitializer(
            Ext1Wakeup,
            ('mask', mask),
            ('wakeup_mode', EXT1_WAKEUP_MODES[conf[CONF_MODE]])
        )
        add(deep_sleep.set_ext1_wakeup(struct))

    setup_component(deep_sleep, config)


BUILD_FLAGS = '-DUSE_DEEP_SLEEP'

CONF_DEEP_SLEEP_ENTER = 'deep_sleep.enter'
DEEP_SLEEP_ENTER_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(DeepSleepComponent),
})


@ACTION_REGISTRY.register(CONF_DEEP_SLEEP_ENTER, DEEP_SLEEP_ENTER_ACTION_SCHEMA)
def deep_sleep_enter_to_code(config, action_id, arg_type, template_arg):
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
def deep_sleep_prevent_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_prevent_deep_sleep_action(template_arg)
    type = PreventDeepSleepAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)
