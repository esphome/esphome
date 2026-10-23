from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ALLOW_OTHER_USES, CONF_INTERRUPT_PIN, CONF_INVERTED
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


INTERRUPT_PIN_SCHEMA = cv.Schema(
    {cv.Optional(CONF_INTERRUPT_PIN): validate_interrupt_pin}
)


async def register_interrupt_pin(var: MockObj, config: ConfigType) -> None:
    """Set the optional interrupt pin of a CachedGpioExpanderComponent."""
    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))
