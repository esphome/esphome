import esphome.codegen as cg
from esphome.components import time
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.const import (
    CONF_HOUR,
    CONF_ID,
    CONF_MINUTE,
    CONF_MODE,
    CONF_NUMBER,
    CONF_PINS,
    CONF_RUN_DURATION,
    CONF_SECOND,
    CONF_SLEEP_DURATION,
    CONF_TIME_ID,
    CONF_WAKEUP_PIN,
)

from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32S2,
)

WAKEUP_PINS = {
    VARIANT_ESP32: [
        0,
        2,
        4,
        12,
        13,
        14,
        15,
        25,
        26,
        27,
        32,
        33,
        34,
        35,
        36,
        37,
        38,
        39,
    ],
    VARIANT_ESP32C3: [0, 1, 2, 3, 4, 5],
    VARIANT_ESP32S2: [
        0,
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        8,
        9,
        10,
        11,
        12,
        13,
        14,
        15,
        16,
        17,
        18,
        19,
        20,
        21,
    ],
}


def validate_pin_number(value):
    valid_pins = WAKEUP_PINS.get(get_esp32_variant(), WAKEUP_PINS[VARIANT_ESP32])
    if value[CONF_NUMBER] not in valid_pins:
        raise cv.Invalid(
            f"Only pins {', '.join(str(x) for x in valid_pins)} support wakeup"
        )
    return value


def validate_config(config):
    if get_esp32_variant() == VARIANT_ESP32C3 and CONF_ESP32_EXT1_WAKEUP in config:
        raise cv.Invalid("ESP32-C3 does not support wakeup from touch.")
    if get_esp32_variant() == VARIANT_ESP32C3 and CONF_TOUCH_WAKEUP in config:
        raise cv.Invalid("ESP32-C3 does not support wakeup from ext1")
    return config


deep_sleep_ns = cg.esphome_ns.namespace("deep_sleep")
DeepSleepComponent = deep_sleep_ns.class_("DeepSleepComponent", cg.Component)
EnterDeepSleepAction = deep_sleep_ns.class_("EnterDeepSleepAction", automation.Action)
PreventDeepSleepAction = deep_sleep_ns.class_(
    "PreventDeepSleepAction",
    automation.Action,
    cg.Parented.template(DeepSleepComponent),
)
AllowDeepSleepAction = deep_sleep_ns.class_(
    "AllowDeepSleepAction",
    automation.Action,
    cg.Parented.template(DeepSleepComponent),
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
WakeupCauseToRunDuration = deep_sleep_ns.struct("WakeupCauseToRunDuration")

CONF_WAKEUP_PIN_MODE = "wakeup_pin_mode"
CONF_ESP32_EXT1_WAKEUP = "esp32_ext1_wakeup"
CONF_TOUCH_WAKEUP = "touch_wakeup"
CONF_DEFAULT = "default"
CONF_GPIO_WAKEUP_REASON = "gpio_wakeup_reason"
CONF_TOUCH_WAKEUP_REASON = "touch_wakeup_reason"
CONF_UNTIL = "until"

WAKEUP_CAUSES_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DEFAULT): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TOUCH_WAKEUP_REASON): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_GPIO_WAKEUP_REASON): cv.positive_time_period_milliseconds,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeepSleepComponent),
        cv.Optional(CONF_RUN_DURATION): cv.Any(
            cv.All(cv.only_on_esp32, WAKEUP_CAUSES_SCHEMA),
            cv.positive_time_period_milliseconds,
        ),
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
                        pins.internal_gpio_input_pin_schema, validate_pin_number
                    ),
                    cv.Required(CONF_MODE): cv.enum(EXT1_WAKEUP_MODES, upper=True),
                }
            ),
        ),
        cv.Optional(CONF_TOUCH_WAKEUP): cv.All(cv.only_on_esp32, cv.boolean),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_SLEEP_DURATION in config:
        cg.add(var.set_sleep_duration(config[CONF_SLEEP_DURATION]))
    if CONF_WAKEUP_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_WAKEUP_PIN])
        cg.add(var.set_wakeup_pin(pin))
    if CONF_WAKEUP_PIN_MODE in config:
        cg.add(var.set_wakeup_pin_mode(config[CONF_WAKEUP_PIN_MODE]))
    if CONF_RUN_DURATION in config:
        run_duration_config = config[CONF_RUN_DURATION]
        if not isinstance(run_duration_config, dict):
            cg.add(var.set_run_duration(config[CONF_RUN_DURATION]))
        else:
            default_run_duration = run_duration_config[CONF_DEFAULT]
            wakeup_cause_to_run_duration = cg.StructInitializer(
                WakeupCauseToRunDuration,
                ("default_cause", default_run_duration),
                (
                    "touch_cause",
                    run_duration_config.get(
                        CONF_TOUCH_WAKEUP_REASON, default_run_duration
                    ),
                ),
                (
                    "gpio_cause",
                    run_duration_config.get(
                        CONF_GPIO_WAKEUP_REASON, default_run_duration
                    ),
                ),
            )
            cg.add(var.set_run_duration(wakeup_cause_to_run_duration))

    if CONF_ESP32_EXT1_WAKEUP in config:
        conf = config[CONF_ESP32_EXT1_WAKEUP]
        mask = 0
        for pin in conf[CONF_PINS]:
            mask |= 1 << pin[CONF_NUMBER]
        struct = cg.StructInitializer(
            Ext1Wakeup, ("mask", mask), ("wakeup_mode", conf[CONF_MODE])
        )
        cg.add(var.set_ext1_wakeup(struct))

    if CONF_TOUCH_WAKEUP in config:
        cg.add(var.set_touch_wakeup(config[CONF_TOUCH_WAKEUP]))

    cg.add_define("USE_DEEP_SLEEP")


DEEP_SLEEP_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DeepSleepComponent),
    }
)

DEEP_SLEEP_ENTER_SCHEMA = cv.All(
    automation.maybe_simple_id(
        DEEP_SLEEP_ACTION_SCHEMA.extend(
            cv.Schema(
                {
                    cv.Exclusive(CONF_SLEEP_DURATION, "time"): cv.templatable(
                        cv.positive_time_period_milliseconds
                    ),
                    # Only on ESP32 due to how long the RTC on ESP8266 can stay asleep
                    cv.Exclusive(CONF_UNTIL, "time"): cv.All(
                        cv.only_on_esp32, cv.time_of_day
                    ),
                    cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
                }
            )
        )
    ),
    cv.has_none_or_all_keys(CONF_UNTIL, CONF_TIME_ID),
)


@automation.register_action(
    "deep_sleep.enter", EnterDeepSleepAction, DEEP_SLEEP_ENTER_SCHEMA
)
async def deep_sleep_enter_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_SLEEP_DURATION in config:
        template_ = await cg.templatable(config[CONF_SLEEP_DURATION], args, cg.int32)
        cg.add(var.set_sleep_duration(template_))

    if CONF_UNTIL in config:
        until = config[CONF_UNTIL]
        cg.add(var.set_until(until[CONF_HOUR], until[CONF_MINUTE], until[CONF_SECOND]))

        time_ = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time(time_))

    return var


@automation.register_action(
    "deep_sleep.prevent",
    PreventDeepSleepAction,
    automation.maybe_simple_id(DEEP_SLEEP_ACTION_SCHEMA),
)
@automation.register_action(
    "deep_sleep.allow",
    AllowDeepSleepAction,
    automation.maybe_simple_id(DEEP_SLEEP_ACTION_SCHEMA),
)
async def deep_sleep_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
