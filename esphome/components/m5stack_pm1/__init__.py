from dataclasses import dataclass

from esphome import pins
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT,
    CONF_INTERRUPT_PIN,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["gpio_expander"]
CODEOWNERS = ["@parker-int64"]
DEPENDENCIES = ["i2c"]
DOMAIN = "m5stack_pm1"
MULTI_CONF = True

CONF_M5STACK_PM1_ID = "m5stack_pm1_id"
CONF_FAST_MODE = "fast_mode"
CONF_INTERRUPT_GPIO = "interrupt_gpio"
CONF_WAKEUP = "wakeup"
CONF_WAKEUP_EDGE = "wakeup_edge"

# GPIOs that share a wake interrupt line and cannot both be wake sources.
_WAKE_PIN_PARTNER = {0: 2, 2: 0, 3: 4, 4: 3}
# GPIO IRQ mask bits; a set bit disables that pin's interrupt.
_GPIO_IRQ_MASK = 0x1F


m5stack_pm1_ns = cg.esphome_ns.namespace("m5stack_pm1")

M5StackPM1Component = m5stack_pm1_ns.class_(
    "M5StackPM1Component", cg.Component, i2c.I2CDevice
)
M5StackPM1GPIOPin = m5stack_pm1_ns.class_("M5StackPM1GPIOPin", cg.GPIOPin)


@dataclass
class M5StackPM1HubPins:
    """Wake and interrupt bits of one hub's pins, collected while validating them."""

    wake_enable: int = 0
    wake_rising: int = 0
    interrupt: int = 0


def _hub_pins(hub_id: str) -> M5StackPM1HubPins:
    hubs = CORE.data.setdefault(DOMAIN, {})
    if hub_id not in hubs:
        hubs[hub_id] = M5StackPM1HubPins()
    return hubs[hub_id]


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5StackPM1Component),
            cv.Optional(CONF_FAST_MODE, default=False): cv.boolean,
            cv.Optional(CONF_INTERRUPT_GPIO): cv.int_range(min=0, max=4),
        }
    )
    .extend(gpio_expander.INTERRUPT_EXPANDER_SCHEMA)
    .extend(i2c.i2c_device_schema(0x6E)),
    cv.has_none_or_all_keys(CONF_INTERRUPT_PIN, CONF_INTERRUPT_GPIO),
)


def _final_validate(config: ConfigType) -> ConfigType:
    if config[CONF_FAST_MODE]:
        i2c.final_validate_device_schema(
            DOMAIN, min_frequency="400kHz", max_frequency="400kHz"
        )(config)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config, config[CONF_FAST_MODE])
    hub_pins = _hub_pins(str(config[CONF_ID]))

    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        await cg.add_pin(var.set_interrupt_pin, interrupt_pin)
        interrupt_gpio = config[CONF_INTERRUPT_GPIO]
        # The IRQ output itself stays masked.
        irq_mask = (_GPIO_IRQ_MASK & ~hub_pins.interrupt) | (1 << interrupt_gpio)
        cg.add(var.set_interrupt_gpio(interrupt_gpio, irq_mask))
    if hub_pins.wake_enable:
        cg.add(var.set_wake_config(hub_pins.wake_enable, hub_pins.wake_rising))


def validate_mode(value: ConfigType) -> ConfigType:
    pins.gpio_validate_input_xor_output(value)
    if value[CONF_PULLUP] and value[CONF_PULLDOWN]:
        raise cv.Invalid("Only one of pullup and pulldown can be enabled")
    if value[CONF_OUTPUT] and (value[CONF_PULLUP] or value[CONF_PULLDOWN]):
        raise cv.Invalid("Pull resistors are only supported in input mode")
    if value[CONF_INPUT] and value[CONF_OPEN_DRAIN]:
        raise cv.Invalid("Open-drain is only supported in output mode")
    return value


def validate_pin_options(config: ConfigType) -> ConfigType:
    if config[CONF_INTERRUPT] and not config[CONF_MODE][CONF_INPUT]:
        raise cv.Invalid("GPIO interrupt is only supported in input mode")
    if config[CONF_WAKEUP]:
        if not config[CONF_MODE][CONF_INPUT]:
            raise cv.Invalid("GPIO wakeup is only supported in input mode")
        if config[CONF_NUMBER] == 1:
            raise cv.Invalid("GPIO1 does not support wakeup", path=[CONF_NUMBER])
        if not config[CONF_MODE][CONF_PULLUP] and not config[CONF_MODE][CONF_PULLDOWN]:
            # Keep the idle level opposite the edge that wakes the controller.
            if config.get(CONF_WAKEUP_EDGE) == "RISING":
                config[CONF_MODE][CONF_PULLDOWN] = True
            else:
                config[CONF_MODE][CONF_PULLUP] = True
    elif CONF_WAKEUP_EDGE in config:
        raise cv.Invalid("wakeup_edge requires wakeup to be enabled")
    return config


def validate_pin_usage(pin_config: ConfigType, hub_config: ConfigType) -> None:
    number = pin_config[CONF_NUMBER]
    if number == hub_config.get(CONF_INTERRUPT_GPIO):
        raise cv.Invalid(
            f"GPIO{number} is used as '{CONF_INTERRUPT_GPIO}' and cannot be used as a pin"
        )
    if pin_config[CONF_INTERRUPT] and CONF_INTERRUPT_PIN not in hub_config:
        raise cv.Invalid(
            f"'{CONF_INTERRUPT}: true' requires '{CONF_INTERRUPT_PIN}' to be configured"
        )

    hub_pins = _hub_pins(str(hub_config[CONF_ID]))
    bit = 1 << number
    if pin_config[CONF_INTERRUPT]:
        hub_pins.interrupt |= bit
    if pin_config.get(CONF_WAKEUP):
        partner = _WAKE_PIN_PARTNER[number]
        if hub_pins.wake_enable & (1 << partner):
            first, second = sorted((number, partner))
            raise cv.Invalid(
                f"GPIO{first} and GPIO{second} share a wake interrupt line "
                "and cannot both be wake sources"
            )
        hub_pins.wake_enable |= bit
        if pin_config.get(CONF_WAKEUP_EDGE) == "RISING":
            hub_pins.wake_rising |= bit


M5STACK_PM1_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        M5StackPM1GPIOPin,
        cv.int_range(min=0, max=4),
        modes=[CONF_INPUT, CONF_OUTPUT, CONF_PULLUP, CONF_PULLDOWN, CONF_OPEN_DRAIN],
        mode_validator=validate_mode,
    ).extend(
        {
            cv.Required(CONF_M5STACK_PM1_ID): cv.use_id(M5StackPM1Component),
            cv.Optional(CONF_INTERRUPT, default=False): cv.boolean,
            cv.Optional(CONF_WAKEUP, default=False): cv.boolean,
            cv.Optional(CONF_WAKEUP_EDGE): cv.one_of("FALLING", "RISING", upper=True),
        }
    ),
    validate_pin_options,
)


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_M5STACK_PM1_ID, M5STACK_PM1_PIN_SCHEMA, validate_pin_usage
)
async def m5stack_pm1_pin_to_code(config: ConfigType) -> MockObj:
    return await pins.new_parented_pin(config, CONF_M5STACK_PM1_ID)
