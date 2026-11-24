from esphome import pins
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_I2C_ID,
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
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["gpio_expander"]
CODEOWNERS = ["@parker-int64"]
DEPENDENCIES = ["i2c"]
DOMAIN = "m5stack_pm1"
MULTI_CONF = True

CONF_M5STACK_PM1_ID = "m5stack_pm1_id"
CONF_INTERRUPT_GPIO = "interrupt_gpio"
CONF_WAKEUP = "wakeup"
CONF_WAKEUP_EDGE = "wakeup_edge"


def _bus_config(bus_id: ID) -> ConfigType | None:
    for bus in CORE.config.get(i2c.DOMAIN, []):
        if bus[CONF_ID].id == bus_id.id:
            return bus
    return None


m5stack_pm1_ns = cg.esphome_ns.namespace("m5stack_pm1")

M5StackPM1Component = m5stack_pm1_ns.class_(
    "M5StackPM1Component", cg.Component, i2c.I2CDevice
)
M5StackPM1GPIOPin = m5stack_pm1_ns.class_("M5StackPM1GPIOPin", cg.GPIOPin)

M5StackPM1WakeupEdge = m5stack_pm1_ns.enum("M5StackPM1WakeupEdge", is_class=True)
WAKEUP_EDGES = M5StackPM1WakeupEdge.options(
    "FALLING", "RISING", prefix="M5STACK_PM1_WAKEUP_EDGE_"
)


def validate_interrupt_config(config: ConfigType) -> ConfigType:
    if (CONF_INTERRUPT_PIN in config) != (CONF_INTERRUPT_GPIO in config):
        raise cv.Invalid(
            f"{CONF_INTERRUPT_PIN} and {CONF_INTERRUPT_GPIO} must be configured together"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5StackPM1Component),
            cv.Optional(CONF_INTERRUPT_GPIO): cv.int_range(min=0, max=4),
        }
    )
    .extend(gpio_expander.INTERRUPT_EXPANDER_SCHEMA)
    .extend(i2c.i2c_device_schema(0x6E)),
    validate_interrupt_config,
)


async def to_code(config: ConfigType) -> None:
    bus = _bus_config(config[CONF_I2C_ID])
    bus_frequency = int(bus[CONF_FREQUENCY]) if bus else 0
    var = await i2c.new_i2c_component(config, bus_frequency)

    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        await cg.add_pin(var.set_interrupt_pin, interrupt_pin)
        cg.add(var.set_interrupt_gpio(config[CONF_INTERRUPT_GPIO]))


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
    if config[CONF_INTERRUPT] and not config[CONF_MODE][CONF_INPUT]:
        raise cv.Invalid("GPIO interrupt is only supported in input mode")
    if config[CONF_WAKEUP]:
        if not config[CONF_MODE][CONF_INPUT]:
            raise cv.Invalid("GPIO wakeup is only supported in input mode")
        if config[CONF_NUMBER] == 1:
            raise cv.Invalid("GPIO1 does not support wakeup", path=[CONF_NUMBER])
        if not config[CONF_MODE][CONF_PULLUP] and not config[CONF_MODE][CONF_PULLDOWN]:
            config[CONF_MODE][CONF_PULLUP] = True
    elif CONF_WAKEUP_EDGE in config:
        raise cv.Invalid("wakeup_edge requires wakeup to be enabled")
    return config


def validate_pin_usage(pin_config: ConfigType, hub_config: ConfigType) -> None:
    if pin_config[CONF_NUMBER] == hub_config.get(CONF_INTERRUPT_GPIO):
        raise cv.Invalid(
            f"GPIO{pin_config[CONF_NUMBER]} is used as '{CONF_INTERRUPT_GPIO}' "
            "and cannot be used as a pin"
        )
    if pin_config[CONF_INTERRUPT] and CONF_INTERRUPT_PIN not in hub_config:
        raise cv.Invalid(
            f"'{CONF_INTERRUPT}: true' requires '{CONF_INTERRUPT_PIN}' to be configured"
        )


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
            cv.Optional(CONF_WAKEUP_EDGE): cv.enum(WAKEUP_EDGES, upper=True),
        }
    ),
    validate_pin_options,
)


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_M5STACK_PM1_ID, M5STACK_PM1_PIN_SCHEMA, validate_pin_usage
)
async def m5stack_pm1_pin_to_code(config: ConfigType) -> MockObj:
    var = await pins.new_parented_pin(config, CONF_M5STACK_PM1_ID)
    parent = await cg.get_variable(config[CONF_M5STACK_PM1_ID])
    if config[CONF_WAKEUP]:
        cg.add(
            parent.set_pin_wakeup(
                config[CONF_NUMBER],
                config.get(CONF_WAKEUP_EDGE, WAKEUP_EDGES["FALLING"]),
            )
        )
    if config[CONF_MODE].get(CONF_INPUT, False):
        cg.add(
            parent.set_gpio_interrupt_enabled(
                config[CONF_NUMBER], config[CONF_INTERRUPT]
            )
        )
    return var
