from esphome import pins
import esphome.config_validation as cv
from esphome.const import CONF_ALLOW_OTHER_USES, CONF_INTERRUPT_PIN, CONF_INVERTED
from esphome.types import ConfigType


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
