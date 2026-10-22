from typing import Any

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c
from esphome.components.const import CONF_PWM_FREQUENCY
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION,
    CONF_DURATION,
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT,
    CONF_INTERRUPT_PIN,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_ON_ERROR,
    CONF_ON_SUCCESS,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

AUTO_LOAD = ["gpio_expander"]
CODEOWNERS = ["@parker-int64"]
DEPENDENCIES = ["i2c"]
DOMAIN = "m5pm1"
MULTI_CONF = True

CONF_M5PM1_ID = "m5pm1_id"
CONF_INTERRUPT_GPIO = "interrupt_gpio"
CONF_WAKEUP = "wakeup"
CONF_WAKEUP_EDGE = "wakeup_edge"
CONF_POWER_BUTTON = "power_button"
CONF_SINGLE_CLICK_TIME = "single_click_time"
CONF_DOUBLE_CLICK_TIME = "double_click_time"
CONF_LONG_PRESS_TIME = "long_press_time"
CONF_SINGLE_CLICK_RESET = "single_click_reset"
CONF_DOUBLE_CLICK_POWEROFF = "double_click_poweroff"
CONF_LONG_PRESS_DOWNLOAD = "long_press_download"
CONF_SINGLE_CLICK_IRQ = "single_click_irq"
CONF_DOUBLE_CLICK_IRQ = "double_click_irq"

M5PM1_TIMER_MAX_SECONDS = 214_748_364

m5pm1_ns = cg.esphome_ns.namespace("m5pm1")

M5PM1Component = m5pm1_ns.class_("M5PM1Component", cg.Component, i2c.I2CDevice)
M5PM1GPIOPin = m5pm1_ns.class_("M5PM1GPIOPin", cg.GPIOPin)
M5PM1TimerStartAction = m5pm1_ns.class_("M5PM1TimerStartAction", automation.Action)
M5PM1TimerClearAction = m5pm1_ns.class_("M5PM1TimerClearAction", automation.Action)
M5PM1SystemCommandAction = m5pm1_ns.class_(
    "M5PM1SystemCommandAction", automation.Action
)

M5PM1PWMPolarity = m5pm1_ns.enum("M5PM1PWMPolarity", is_class=True)
PWM_POLARITIES = M5PM1PWMPolarity.options(
    "ACTIVE_HIGH", "ACTIVE_LOW", prefix="M5PM1_PWM_POLARITY_"
)

M5PM1PWMChannel = m5pm1_ns.enum("M5PM1PWMChannel", is_class=True)
PWM_CHANNELS = {
    "PWM_1": M5PM1PWMChannel.M5PM1_PWM_CHANNEL_1,
    "PWM_2": M5PM1PWMChannel.M5PM1_PWM_CHANNEL_2,
}
M5PM1ADCChannel = m5pm1_ns.enum("M5PM1ADCChannel", is_class=True)
ADC_CHANNELS = {
    "ADC_1": M5PM1ADCChannel.M5PM1_ADC_CHANNEL_1,
    "ADC_2": M5PM1ADCChannel.M5PM1_ADC_CHANNEL_2,
    "TEMPERATURE": M5PM1ADCChannel.M5PM1_ADC_CHANNEL_TEMPERATURE,
}

M5PM1TimerAction = m5pm1_ns.enum("M5PM1TimerAction", is_class=True)
TIMER_ACTIONS = M5PM1TimerAction.options(
    "SET_WAKEUP_FLAG", "REBOOT", "POWER_ON", "POWER_OFF", prefix="M5PM1_TIMER_ACTION_"
)

M5PM1WakeupEdge = m5pm1_ns.enum("M5PM1WakeupEdge", is_class=True)
WAKEUP_EDGES = M5PM1WakeupEdge.options("FALLING", "RISING", prefix="M5PM1_WAKEUP_EDGE_")

M5PM1SystemCommand = m5pm1_ns.enum("M5PM1SystemCommand", is_class=True)
SYSTEM_COMMANDS = {
    "m5pm1.power_off": M5PM1SystemCommand.M5PM1_SYSTEM_COMMAND_POWER_OFF,
    "m5pm1.reboot": M5PM1SystemCommand.M5PM1_SYSTEM_COMMAND_REBOOT,
    "m5pm1.enter_download_mode": M5PM1SystemCommand.M5PM1_SYSTEM_COMMAND_DOWNLOAD,
}

M5PM1ButtonTiming = m5pm1_ns.enum("M5PM1ButtonTiming", is_class=True)
BUTTON_TIMINGS = {
    125: M5PM1ButtonTiming.M5PM1_BUTTON_TIMING_125_MS,
    250: M5PM1ButtonTiming.M5PM1_BUTTON_TIMING_250_MS,
    500: M5PM1ButtonTiming.M5PM1_BUTTON_TIMING_500_MS,
    1000: M5PM1ButtonTiming.M5PM1_BUTTON_TIMING_1000_MS,
}

M5PM1LongPressTiming = m5pm1_ns.enum("M5PM1LongPressTiming", is_class=True)
LONG_PRESS_TIMINGS = {
    1000: M5PM1LongPressTiming.M5PM1_LONG_PRESS_TIMING_1_S,
    2000: M5PM1LongPressTiming.M5PM1_LONG_PRESS_TIMING_2_S,
    3000: M5PM1LongPressTiming.M5PM1_LONG_PRESS_TIMING_3_S,
    4000: M5PM1LongPressTiming.M5PM1_LONG_PRESS_TIMING_4_S,
}

TIMER_CALLBACK_SCHEMA = {
    cv.Optional(CONF_ON_SUCCESS): automation.validate_automation(single=True),
    cv.Optional(CONF_ON_ERROR): automation.validate_automation(single=True),
}

TIMER_START_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(M5PM1Component),
        cv.Required(CONF_DURATION): cv.templatable(
            cv.All(
                cv.positive_time_period_seconds,
                cv.Range(
                    min=cv.TimePeriod(seconds=1),
                    max=cv.TimePeriod(seconds=M5PM1_TIMER_MAX_SECONDS),
                ),
            )
        ),
        cv.Required(CONF_ACTION): cv.templatable(cv.enum(TIMER_ACTIONS, upper=True)),
        **TIMER_CALLBACK_SCHEMA,
    }
)

TIMER_CLEAR_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(M5PM1Component),
        **TIMER_CALLBACK_SCHEMA,
    }
)

SYSTEM_COMMAND_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(M5PM1Component),
        **TIMER_CALLBACK_SCHEMA,
    }
)


def validate_interrupt_config(config: ConfigType) -> ConfigType:
    if (CONF_INTERRUPT_PIN in config) != (CONF_INTERRUPT_GPIO in config):
        raise cv.Invalid(
            f"{CONF_INTERRUPT_PIN} and {CONF_INTERRUPT_GPIO} must be configured together"
        )
    return config


def _validate_button_timing(value: Any) -> MockObj:
    value = cv.positive_time_period_milliseconds(value)
    return cv.enum(BUTTON_TIMINGS, int=True)(value.total_milliseconds)


def _validate_long_press_timing(value: Any) -> MockObj:
    value = cv.positive_time_period_milliseconds(value)
    return cv.enum(LONG_PRESS_TIMINGS, int=True)(value.total_milliseconds)


POWER_BUTTON_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SINGLE_CLICK_TIME, default="250ms"): _validate_button_timing,
        cv.Optional(CONF_DOUBLE_CLICK_TIME, default="500ms"): _validate_button_timing,
        cv.Optional(CONF_LONG_PRESS_TIME, default="1s"): _validate_long_press_timing,
        cv.Optional(CONF_SINGLE_CLICK_RESET, default=True): cv.boolean,
        cv.Optional(CONF_DOUBLE_CLICK_POWEROFF, default=True): cv.boolean,
        cv.Optional(CONF_LONG_PRESS_DOWNLOAD, default=True): cv.boolean,
        cv.Optional(CONF_SINGLE_CLICK_IRQ, default=True): cv.boolean,
        cv.Optional(CONF_DOUBLE_CLICK_IRQ, default=True): cv.boolean,
    }
)


BASE_SCHEMA = cv.Schema({cv.GenerateID(CONF_M5PM1_ID): cv.use_id(M5PM1Component)})

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(M5PM1Component),
            cv.Optional(CONF_INTERRUPT_PIN): gpio_expander.validate_interrupt_pin,
            cv.Optional(CONF_INTERRUPT_GPIO): cv.int_range(min=0, max=4),
            cv.Optional(CONF_PWM_FREQUENCY, default="500Hz"): cv.All(
                cv.frequency, cv.int_range(min=0, max=65535)
            ),
            cv.Optional(CONF_POWER_BUTTON, default={}): POWER_BUTTON_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x6E)),
    validate_interrupt_config,
)

M5PM1_GPIO_OUTPUT_PIN_SCHEMA = pins.gpio_pin_schema({CONF_OUTPUT: True})

M5PM1_GPIO_INPUT_PIN_SCHEMA = pins.gpio_pin_schema({CONF_INPUT: True})


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin)))
        cg.add(var.set_interrupt_gpio(config[CONF_INTERRUPT_GPIO]))
    cg.add(var.set_pwm_frequency(config[CONF_PWM_FREQUENCY]))
    power_button = config[CONF_POWER_BUTTON]
    cg.add(var.set_single_click_timing(power_button[CONF_SINGLE_CLICK_TIME]))
    cg.add(var.set_double_click_timing(power_button[CONF_DOUBLE_CLICK_TIME]))
    cg.add(var.set_long_press_timing(power_button[CONF_LONG_PRESS_TIME]))
    cg.add(var.set_single_click_reset(power_button[CONF_SINGLE_CLICK_RESET]))
    cg.add(var.set_double_click_poweroff(power_button[CONF_DOUBLE_CLICK_POWEROFF]))
    cg.add(var.set_long_press_download(power_button[CONF_LONG_PRESS_DOWNLOAD]))
    cg.add(var.set_single_click_interrupt_enabled(power_button[CONF_SINGLE_CLICK_IRQ]))
    cg.add(var.set_double_click_interrupt_enabled(power_button[CONF_DOUBLE_CLICK_IRQ]))


async def _build_action_callbacks(var: MockObj, config: ConfigType) -> None:
    if on_success := config.get(CONF_ON_SUCCESS):
        await automation.build_automation(var.get_success_trigger(), [], on_success)
    if on_error := config.get(CONF_ON_ERROR):
        await automation.build_automation(var.get_error_trigger(), [], on_error)


@automation.register_action(
    "m5pm1.timer.start",
    M5PM1TimerStartAction,
    TIMER_START_ACTION_SCHEMA,
    synchronous=True,
)
async def m5pm1_timer_start_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    duration = await cg.templatable(config[CONF_DURATION], args, cg.uint32)
    cg.add(var.set_duration(duration))
    timer_action = await cg.templatable(config[CONF_ACTION], args, M5PM1TimerAction)
    cg.add(var.set_timer_action(timer_action))
    await _build_action_callbacks(var, config)
    return var


@automation.register_action(
    "m5pm1.timer.clear",
    M5PM1TimerClearAction,
    TIMER_CLEAR_ACTION_SCHEMA,
    synchronous=True,
)
async def m5pm1_timer_clear_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    await _build_action_callbacks(var, config)
    return var


async def _system_command_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    command: MockObj,
) -> MockObj:
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent, command)
    await _build_action_callbacks(var, config)
    return var


@automation.register_action(
    "m5pm1.power_off",
    M5PM1SystemCommandAction,
    SYSTEM_COMMAND_ACTION_SCHEMA,
    synchronous=True,
)
async def m5pm1_power_off_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _system_command_action_to_code(
        config, action_id, template_arg, SYSTEM_COMMANDS["m5pm1.power_off"]
    )


@automation.register_action(
    "m5pm1.reboot",
    M5PM1SystemCommandAction,
    SYSTEM_COMMAND_ACTION_SCHEMA,
    synchronous=True,
)
async def m5pm1_reboot_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _system_command_action_to_code(
        config, action_id, template_arg, SYSTEM_COMMANDS["m5pm1.reboot"]
    )


@automation.register_action(
    "m5pm1.enter_download_mode",
    M5PM1SystemCommandAction,
    SYSTEM_COMMAND_ACTION_SCHEMA,
    synchronous=True,
)
async def m5pm1_enter_download_mode_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _system_command_action_to_code(
        config,
        action_id,
        template_arg,
        SYSTEM_COMMANDS["m5pm1.enter_download_mode"],
    )


def validate_mode(value: ConfigType) -> ConfigType:
    if value[CONF_INPUT] == value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_PULLUP] and value[CONF_PULLDOWN]:
        raise cv.Invalid("Only one of pullup and pulldown can be enabled")
    if value[CONF_OUTPUT] and (value[CONF_PULLUP] or value[CONF_PULLDOWN]):
        raise cv.Invalid("Pull resistors are only supported in input mode")
    if value[CONF_INPUT] and value[CONF_OPEN_DRAIN]:
        raise cv.Invalid("Open-drain is only supported in output mode")
    return value


def validate_pin_options(config: ConfigType) -> ConfigType:
    if config[CONF_WAKEUP]:
        if not config[CONF_MODE][CONF_INPUT]:
            raise cv.Invalid("GPIO wakeup is only supported in input mode")
        if config[CONF_NUMBER] == 1:
            raise cv.Invalid("GPIO1 does not support wakeup", path=[CONF_NUMBER])
    elif CONF_WAKEUP_EDGE in config:
        raise cv.Invalid("wakeup_edge requires wakeup to be enabled")
    return config


M5PM1_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        M5PM1GPIOPin,
        cv.int_range(min=0, max=4),
        modes=[CONF_INPUT, CONF_OUTPUT, CONF_PULLUP, CONF_PULLDOWN, CONF_OPEN_DRAIN],
        mode_validator=validate_mode,
    ).extend(
        {
            cv.Required(CONF_M5PM1_ID): cv.use_id(M5PM1Component),
            cv.Optional(CONF_INTERRUPT, default=False): cv.boolean,
            cv.Optional(CONF_WAKEUP, default=False): cv.boolean,
            cv.Optional(CONF_WAKEUP_EDGE): cv.enum(WAKEUP_EDGES, upper=True),
        }
    ),
    validate_pin_options,
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_M5PM1_ID, M5PM1_PIN_SCHEMA)
async def m5pm1_pin_to_code(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_M5PM1_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    cg.add(var.set_wakeup(config[CONF_WAKEUP]))
    if wakeup_edge := config.get(CONF_WAKEUP_EDGE):
        cg.add(var.set_wakeup_edge(wakeup_edge))
    if config[CONF_MODE].get(CONF_INPUT, False):
        cg.add(
            parent.set_gpio_interrupt_enabled(
                config[CONF_NUMBER], config[CONF_INTERRUPT]
            )
        )
    return var
