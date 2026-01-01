from esphome import automation, core, pins
import esphome.codegen as cg
from esphome.components import esp32, time
from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    get_esp32_variant,
)
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEFAULT,
    CONF_HOUR,
    CONF_ID,
    CONF_MINUTE,
    CONF_MODE,
    CONF_NUMBER,
    CONF_PIN,
    CONF_PINS,
    CONF_RUN_DURATION,
    CONF_SECOND,
    CONF_SLEEP_DURATION,
    CONF_TIME_ID,
    CONF_WAKEUP_PIN,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PlatformFramework,
)
from esphome.core import CORE
from esphome.types import ConfigType

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
    VARIANT_ESP32C2: [0, 1, 2, 3, 4, 5],
    VARIANT_ESP32C3: [0, 1, 2, 3, 4, 5],
    VARIANT_ESP32C5: [0, 1, 2, 3, 4, 5, 6, 7],
    VARIANT_ESP32C6: [0, 1, 2, 3, 4, 5, 6, 7],
    VARIANT_ESP32C61: [0, 1, 2, 3, 4, 5, 6],
    VARIANT_ESP32H2: [7, 8, 9, 10, 11, 12, 13, 14],
    VARIANT_ESP32P4: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
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
    VARIANT_ESP32S3: [
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


def validate_pin_number_esp32(value: ConfigType) -> ConfigType:
    valid_pins = WAKEUP_PINS.get(get_esp32_variant(), WAKEUP_PINS[VARIANT_ESP32])
    if value[CONF_NUMBER] not in valid_pins:
        raise cv.Invalid(
            f"Only pins {', '.join(str(x) for x in valid_pins)} support wakeup"
        )
    return value


def validate_pin_number(value: ConfigType) -> ConfigType:
    if not CORE.is_esp32:
        return value
    return validate_pin_number_esp32(value)


def validate_wakeup_pin(
    value: ConfigType | list[ConfigType],
) -> list[ConfigType]:
    if not isinstance(value, list):
        processed_pins: list[ConfigType] = [{CONF_PIN: value}]
    else:
        processed_pins = list(value)

    for i, pin_config in enumerate(processed_pins):
        # now validate each item
        validated_pin = WAKEUP_PIN_SCHEMA(pin_config)
        validate_pin_number(validated_pin[CONF_PIN])
        processed_pins[i] = validated_pin

    return processed_pins


def validate_config(config: ConfigType) -> ConfigType:
    # right now only BK72XX supports the list format for wakeup pins
    if CORE.is_bk72xx:
        if CONF_WAKEUP_PIN_MODE in config:
            wakeup_pins = config.get(CONF_WAKEUP_PIN, [])
            if len(wakeup_pins) > 1:
                raise cv.Invalid(
                    "You need to remove the global wakeup_pin_mode and define it per pin"
                )
            if wakeup_pins:
                wakeup_pins[0][CONF_WAKEUP_PIN_MODE] = config.pop(CONF_WAKEUP_PIN_MODE)
    elif (
        isinstance(config.get(CONF_WAKEUP_PIN), list)
        and len(config[CONF_WAKEUP_PIN]) > 1
    ):
        raise cv.Invalid(
            "Your platform does not support providing multiple entries in wakeup_pin"
        )

    return config


def _validate_ex1_wakeup_mode(value):
    if value == "ALL_LOW":
        esp32.only_on_variant(supported=[VARIANT_ESP32], msg_prefix="ALL_LOW")(value)
    if value == "ANY_LOW":
        esp32.only_on_variant(
            supported=[
                VARIANT_ESP32C5,
                VARIANT_ESP32C6,
                VARIANT_ESP32C61,
                VARIANT_ESP32H2,
                VARIANT_ESP32P4,
                VARIANT_ESP32S2,
                VARIANT_ESP32S3,
            ],
            msg_prefix="ANY_LOW",
        )(value)
    return value


def _validate_sleep_duration(value: core.TimePeriod) -> core.TimePeriod:
    if not CORE.is_bk72xx:
        return value
    max_duration = core.TimePeriod(hours=36)
    if value > max_duration:
        raise cv.Invalid("sleep duration cannot be more than 36 hours on BK72XX")
    return value


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
    "ANY_LOW": esp_sleep_ext1_wakeup_mode_t.ESP_EXT1_WAKEUP_ANY_LOW,
    "ALL_LOW": esp_sleep_ext1_wakeup_mode_t.ESP_EXT1_WAKEUP_ALL_LOW,
    "ANY_HIGH": esp_sleep_ext1_wakeup_mode_t.ESP_EXT1_WAKEUP_ANY_HIGH,
}
WakeupCauseToRunDuration = deep_sleep_ns.struct("WakeupCauseToRunDuration")

CONF_WAKEUP_PIN_MODE = "wakeup_pin_mode"
CONF_ESP32_EXT1_WAKEUP = "esp32_ext1_wakeup"
CONF_TOUCH_WAKEUP = "touch_wakeup"
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

WAKEUP_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_WAKEUP_PIN_MODE): cv.enum(WAKEUP_PIN_MODES, upper=True),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DeepSleepComponent),
            cv.Optional(CONF_RUN_DURATION): cv.Any(
                cv.All(cv.only_on_esp32, WAKEUP_CAUSES_SCHEMA),
                cv.positive_time_period_milliseconds,
            ),
            cv.Optional(CONF_SLEEP_DURATION): cv.All(
                cv.positive_time_period_milliseconds,
                _validate_sleep_duration,
            ),
            cv.Optional(CONF_WAKEUP_PIN): validate_wakeup_pin,
            cv.Optional(CONF_WAKEUP_PIN_MODE): cv.All(
                cv.only_on([PLATFORM_ESP32, PLATFORM_BK72XX]),
                cv.enum(WAKEUP_PIN_MODES),
                upper=True,
            ),
            cv.Optional(CONF_ESP32_EXT1_WAKEUP): cv.All(
                cv.only_on_esp32,
                esp32.only_on_variant(
                    unsupported=[VARIANT_ESP32C2, VARIANT_ESP32C3],
                    msg_prefix="Wakeup from ext1",
                ),
                cv.Schema(
                    {
                        cv.Required(CONF_PINS): cv.ensure_list(
                            pins.internal_gpio_input_pin_schema,
                            validate_pin_number_esp32,
                        ),
                        cv.Required(CONF_MODE): cv.All(
                            cv.enum(EXT1_WAKEUP_MODES, upper=True),
                            _validate_ex1_wakeup_mode,
                        ),
                    }
                ),
            ),
            cv.Optional(CONF_TOUCH_WAKEUP): cv.All(
                cv.only_on_esp32,
                esp32.only_on_variant(
                    unsupported=[
                        VARIANT_ESP32C2,
                        VARIANT_ESP32C3,
                        VARIANT_ESP32C5,
                        VARIANT_ESP32C6,
                        VARIANT_ESP32C61,
                        VARIANT_ESP32H2,
                    ],
                    msg_prefix="Wakeup from touch",
                ),
                cv.boolean,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_BK72XX]),
    validate_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_SLEEP_DURATION in config:
        cg.add(var.set_sleep_duration(config[CONF_SLEEP_DURATION]))
    if CONF_WAKEUP_PIN in config:
        pins_as_list = config.get(CONF_WAKEUP_PIN, [])
        if CORE.is_bk72xx:
            cg.add(var.init_wakeup_pins_(len(pins_as_list)))
            for item in pins_as_list:
                cg.add(
                    var.add_wakeup_pin(
                        await cg.gpio_pin_expression(item[CONF_PIN]),
                        item.get(
                            CONF_WAKEUP_PIN_MODE, WakeupPinMode.WAKEUP_PIN_MODE_IGNORE
                        ),
                    )
                )
        else:
            pin = await cg.gpio_pin_expression(pins_as_list[0][CONF_PIN])
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
                        cv.All(
                            cv.positive_time_period_milliseconds,
                            _validate_sleep_duration,
                        )
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


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "deep_sleep_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "deep_sleep_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "deep_sleep_bk72xx.cpp": {PlatformFramework.BK72XX_ARDUINO},
    }
)
