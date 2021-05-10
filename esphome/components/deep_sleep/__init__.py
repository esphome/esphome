import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_NUMBER,
    CONF_PINS,
    CONF_RUN_CYCLES,
    CONF_RUN_DURATION,
    CONF_SLEEP_DURATION,
    CONF_WAKEUP_PIN,
)


def validate_pin_number(value):
    valid_pins = [0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 35, 36, 37, 38, 39]
    if value[CONF_NUMBER] not in valid_pins:
        raise cv.Invalid(
            "Only pins {} support wakeup"
            "".format(", ".join(str(x) for x in valid_pins))
        )
    return value


deep_sleep_ns = cg.esphome_ns.namespace("deep_sleep")
DeepSleepComponent = deep_sleep_ns.class_("DeepSleepComponent", cg.Component)
EnterDeepSleepAction = deep_sleep_ns.class_("EnterDeepSleepAction", automation.Action)
PreventDeepSleepAction = deep_sleep_ns.class_(
    "PreventDeepSleepAction", automation.Action
)

WakeupPinMode = deep_sleep_ns.enum("WakeupPinMode")
WAKEUP_PIN_MODES = {
    "IGNORE": WakeupPinMode.WAKEUP_PIN_MODE_IGNORE,
    "KEEP_AWAKE": WakeupPinMode.WAKEUP_PIN_MODE_KEEP_AWAKE,
    "INVERT_WAKEUP": WakeupPinMode.WAKEUP_PIN_MODE_INVERT_WAKEUP,
}

esp_sleep_ext1_wakeup_mode_t = cg.global_ns.enum("esp_sleep_ext1_wakeup_mode_t")
Ext1Wakeup = deep_sleep_ns.struct("Ext1Wakeup")
EXT1_WAKEUP_MODES = {
    "ALL_LOW": esp_sleep_ext1_wakeup_mode_t.ESP_EXT1_WAKEUP_ALL_LOW,
    "ANY_HIGH": esp_sleep_ext1_wakeup_mode_t.ESP_EXT1_WAKEUP_ANY_HIGH,
}

CONF_WAKEUP_PIN_MODE = "wakeup_pin_mode"
CONF_ESP32_EXT1_WAKEUP = "esp32_ext1_wakeup"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeepSleepComponent),
        cv.Optional(CONF_RUN_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SLEEP_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_WAKEUP_PIN): cv.All(
            cv.only_on_esp32, pins.internal_gpio_input_pin_schema, validate_pin_number
        ),
        cv.Optional(CONF_WAKEUP_PIN_MODE): cv.All(
            cv.only_on_esp32, cv.enum(WAKEUP_PIN_MODES), upper=True
        ),
        cv.Optional(CONF_ESP32_EXT1_WAKEUP): cv.All(
            cv.only_on_esp32,
            cv.Schema(
                {
                    cv.Required(CONF_PINS): cv.ensure_list(
                        pins.shorthand_input_pin, validate_pin_number
                    ),
                    cv.Required(CONF_MODE): cv.enum(EXT1_WAKEUP_MODES, upper=True),
                }
            ),
        ),
        cv.Optional(CONF_RUN_CYCLES): cv.invalid(
            "The run_cycles option has been removed in 1.11.0 as "
            "it was essentially the same as a run_duration of 0s."
            "Please use run_duration now."
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    if CONF_SLEEP_DURATION in config:
        cg.add(var.set_sleep_duration(config[CONF_SLEEP_DURATION]))
    if CONF_WAKEUP_PIN in config:
        pin = yield cg.gpio_pin_expression(config[CONF_WAKEUP_PIN])
        cg.add(var.set_wakeup_pin(pin))
    if CONF_WAKEUP_PIN_MODE in config:
        cg.add(var.set_wakeup_pin_mode(config[CONF_WAKEUP_PIN_MODE]))
    if CONF_RUN_DURATION in config:
        cg.add(var.set_run_duration(config[CONF_RUN_DURATION]))

    if CONF_ESP32_EXT1_WAKEUP in config:
        conf = config[CONF_ESP32_EXT1_WAKEUP]
        mask = 0
        for pin in conf[CONF_PINS]:
            mask |= 1 << pin[CONF_NUMBER]
        struct = cg.StructInitializer(
            Ext1Wakeup, ("mask", mask), ("wakeup_mode", conf[CONF_MODE])
        )
        cg.add(var.set_ext1_wakeup(struct))

    cg.add_define("USE_DEEP_SLEEP")


DEEP_SLEEP_ENTER_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(DeepSleepComponent),
        cv.Optional(CONF_SLEEP_DURATION): cv.positive_time_period_milliseconds,
    }
)


DEEP_SLEEP_PREVENT_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(DeepSleepComponent),
    }
)


@automation.register_action(
    "deep_sleep.enter", EnterDeepSleepAction, DEEP_SLEEP_ENTER_SCHEMA
)
def deep_sleep_enter_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_SLEEP_DURATION in config:
        template_ = yield cg.templatable(config[CONF_SLEEP_DURATION], args, cg.int32)
        cg.add(var.set_sleep_duration(template_))
    yield var


@automation.register_action(
    "deep_sleep.prevent", PreventDeepSleepAction, DEEP_SLEEP_PREVENT_SCHEMA
)
def deep_sleep_prevent_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)
