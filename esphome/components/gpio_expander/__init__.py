from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALLOW_OTHER_USES,
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT_PIN,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PULLUP,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

DOMAIN = "gpio_expander"


def validate_interrupt_pin(value: ConfigType) -> ConfigType:
    # The expander components own INT polarity (active-low, hardcoded falling-edge ISR)
    # and install a single ISR per GPIO, so neither inversion nor sharing is supported.
    value = pins.internal_gpio_input_pin_schema(value)
    if value.get(CONF_INVERTED):
        raise cv.Invalid(
            f"'{CONF_INVERTED}: true' is not supported on '{CONF_INTERRUPT_PIN}'; "
            "the expander INT line is fixed active-low"
        )
    if value.get(CONF_ALLOW_OTHER_USES):
        raise cv.Invalid(
            f"'{CONF_ALLOW_OTHER_USES}: true' is not supported on '{CONF_INTERRUPT_PIN}'; "
            "sharing the interrupt pin between multiple components is not implemented. "
            f"Remove the '{CONF_INTERRUPT_PIN}' to fall back to polling."
        )
    return value


def validate_input_xor_output(value: ConfigType) -> ConfigType:
    """Pin mode validator: exactly one of input or output."""
    if value[CONF_INPUT] == value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


def validate_input_xor_output_pullup(value: ConfigType) -> ConfigType:
    """``validate_input_xor_output`` for chips whose pull-up only works on inputs."""
    validate_input_xor_output(value)
    if value[CONF_PULLUP] and not value[CONF_INPUT]:
        raise cv.Invalid("Pullup only available with input")
    return value


async def new_expander_pin(config: ConfigType, parent_key: str) -> MockObj:
    """Create an expander pin (``ExpanderGPIOPin`` or compatible) parented to ``config[parent_key]``."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[parent_key])
    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
