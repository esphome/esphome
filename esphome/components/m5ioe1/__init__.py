from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT_PIN,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PIN,
    CONF_PULLDOWN,
    CONF_PULLUP,
    CONF_RESET,
)
import esphome.final_validate as fv

AUTO_LOAD = ["gpio_expander"]
CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True


m5ioe1_ns = cg.esphome_ns.namespace("m5ioe1")
M5IOE1Component = m5ioe1_ns.class_("M5IOE1Component", cg.Component, i2c.I2CDevice)
M5IOE1GPIOPin = m5ioe1_ns.class_("M5IOE1GPIOPin", cg.GPIOPin)

CONF_M5IOE1_ID = "m5ioe1_id"
CONF_PWM_FREQUENCY = "pwm_frequency"
CONF_PWM_POLARITY = "pwm_polarity"

M5IOE1PWMPolarity = m5ioe1_ns.enum("M5IOE1PWMPolarity")

PWM_POLARITY = {
    "ACTIVE_HIGH": M5IOE1PWMPolarity.ACTIVE_HIGH,
    "ACTIVE_LOW": M5IOE1PWMPolarity.ACTIVE_LOW,
}

M5IOE1PWMChannel = m5ioe1_ns.enum("M5IOE1PWMChannel")

PWM_CHANNEL = {
    "PWM_1": M5IOE1PWMChannel.PWM_1,
    "PWM_2": M5IOE1PWMChannel.PWM_2,
    "PWM_3": M5IOE1PWMChannel.PWM_3,
    "PWM_4": M5IOE1PWMChannel.PWM_4,
}

M5IOE1ADCChannel = m5ioe1_ns.enum("M5IOE1ADCChannel")

ADC_CHANNEL = {
    "ADC_1": M5IOE1ADCChannel.ADC_1,
    "ADC_2": M5IOE1ADCChannel.ADC_2,
    "ADC_3": M5IOE1ADCChannel.ADC_3,
    "ADC_4": M5IOE1ADCChannel.ADC_4,
    "REF_VOLT": M5IOE1ADCChannel.REF_VOLT,
}

CONF_USE_INTERRUPT = "use_interrupt"
CONF_INTERRUPT_TYPE = "interrupt_type"

M5IOE1InterruptType = m5ioe1_ns.enum("M5IOE1InterruptType")

INTERRUPT_TYPES = {
    "RISING_EDGE": M5IOE1InterruptType.RISING_EDGE,
    "FALLING_EDGE": M5IOE1InterruptType.FALLING_EDGE,
}

INTERRUPT_CONFLICT_PAIRS = [{0, 5}, {1, 2}, {6, 11}, {7, 8}, {9, 13}, {10, 12}]

BASE_SCHEMA = cv.Schema({cv.GenerateID(CONF_M5IOE1_ID): cv.use_id(M5IOE1Component)})


# Interrupt conflict validator — runs after the full config is loaded
def final_validate_config(config):
    hub_id = str(config[CONF_ID])
    enabled_interrupt_pins = []

    full_config = fv.full_config.get()

    for component_list in full_config.values():
        if not isinstance(component_list, list):
            component_list = [component_list]

        for item in component_list:
            if not isinstance(item, dict) or CONF_PIN not in item:
                continue

            pin_conf = item[CONF_PIN]
            if (
                isinstance(pin_conf, dict)
                and str(pin_conf.get(CONF_M5IOE1_ID, "")) == hub_id
                and pin_conf.get(CONF_USE_INTERRUPT)
            ):
                enabled_interrupt_pins.append(pin_conf[CONF_NUMBER])

    for p1, p2 in [tuple(pair) for pair in INTERRUPT_CONFLICT_PAIRS]:
        if p1 in enabled_interrupt_pins and p2 in enabled_interrupt_pins:
            raise cv.Invalid(
                f"GPIO{p1 + 1} (pin number: {p1}) and GPIO{p2 + 1} (pin number: {p2}) interrupt cannot be enabled simultaneously "
                f"on hub '{hub_id}' (they share the same interrupt channel)."
            )

    return config


FINAL_VALIDATE_SCHEMA = final_validate_config

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(M5IOE1Component),
            cv.Optional(CONF_RESET, default=True): cv.boolean,
            cv.Optional(CONF_PWM_FREQUENCY, default="500Hz"): cv.All(
                cv.frequency, cv.int_range(min=1, max=65535)
            ),
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x6F))
)


def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


M5IOE1_PIN_SCHEMA = pins.gpio_base_schema(
    M5IOE1GPIOPin,
    cv.int_range(min=0, max=13),
    modes=[CONF_INPUT, CONF_OUTPUT, CONF_PULLUP, CONF_PULLDOWN, CONF_OPEN_DRAIN],
    mode_validator=validate_mode,
).extend(
    {
        cv.Required(CONF_M5IOE1_ID): cv.use_id(M5IOE1Component),
        cv.Optional(CONF_USE_INTERRUPT, default=False): cv.boolean,
        cv.Optional(CONF_INTERRUPT_TYPE, default="FALLING_EDGE"): cv.enum(
            INTERRUPT_TYPES, upper=True, space="_"
        ),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_M5IOE1_ID, M5IOE1_PIN_SCHEMA)
async def m5ioe1_pin_schema(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_M5IOE1_ID])

    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    cg.add(var.set_use_interrupt(config[CONF_USE_INTERRUPT]))
    cg.add(var.set_interrupt_type(config[CONF_INTERRUPT_TYPE]))
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_reset(config[CONF_RESET]))

    if CONF_PWM_FREQUENCY in config:
        cg.add(var.set_pwm_frequency(config[CONF_PWM_FREQUENCY]))

    if CONF_INTERRUPT_PIN in config:
        cg.add(
            var.set_interrupt_pin(
                await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
            )
        )
