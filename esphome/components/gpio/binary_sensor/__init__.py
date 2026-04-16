import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALLOW_OTHER_USES,
    CONF_ID,
    CONF_NAME,
    CONF_NUMBER,
    CONF_PIN,
)
from esphome.core import CORE

from .. import gpio_ns

_LOGGER = logging.getLogger(__name__)

GPIOBinarySensor = gpio_ns.class_(
    "GPIOBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONF_USE_INTERRUPT = "use_interrupt"
CONF_INTERRUPT_TYPE = "interrupt_type"

INTERRUPT_TYPES = {
    "RISING": gpio_ns.INTERRUPT_RISING_EDGE,
    "FALLING": gpio_ns.INTERRUPT_FALLING_EDGE,
    "ANY": gpio_ns.INTERRUPT_ANY_EDGE,
}

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(GPIOBinarySensor)
    .extend(
        {
            cv.Required(CONF_PIN): pins.gpio_input_pin_schema,
            # Interrupts are disabled by default for bk72xx, ln882x, and rtl87xx platforms
            # due to hardware limitations or lack of reliable interrupt support. This ensures
            # stable operation on these platforms. Future maintainers should verify platform
            # capabilities before changing this default behavior.
            # nrf52 has no gpio interrupts implemented yet
            cv.SplitDefault(
                CONF_USE_INTERRUPT,
                bk72xx=False,
                esp32=True,
                esp8266=True,
                host=True,
                ln882x=False,
                nrf52=False,
                rp2040=True,
                rtl87xx=False,
            ): cv.boolean,
            cv.Optional(CONF_INTERRUPT_TYPE, default="ANY"): cv.enum(
                INTERRUPT_TYPES, upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def _pin_is_deep_sleep_wakeup(pin_num: int) -> bool:
    """Check if pin is configured as a deep_sleep wakeup pin."""
    if not CORE.config or "deep_sleep" not in CORE.config:
        return False

    from esphome.components.deep_sleep import is_wakeup_pin

    return is_wakeup_pin(pin_num)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    # Check for ESP8266 GPIO16 interrupt limitation
    # GPIO16 on ESP8266 is a special pin that doesn't support interrupts through
    # the Arduino attachInterrupt() function. This is the only known GPIO pin
    # across all supported platforms that has this limitation, so we handle it
    # here instead of in the platform-specific code.
    use_interrupt = config[CONF_USE_INTERRUPT]
    pin_num = config[CONF_PIN][CONF_NUMBER]

    # Expander pins (e.g. PCF8574, MCP23017) don't support direct interrupt
    # attachment — only internal/native GPIO pins do.
    is_internal = (
        pins.PIN_SCHEMA_REGISTRY.get_key(config[CONF_PIN]) == CORE.target_platform
    )
    if use_interrupt and not is_internal:
        _LOGGER.info(
            "GPIO binary_sensor '%s': Pin is not an internal GPIO, "
            "falling back to polling mode.",
            config.get(CONF_NAME, config[CONF_ID]),
        )
        use_interrupt = False

    if use_interrupt and CORE.is_esp8266 and pin_num == 16:
        _LOGGER.warning(
            "GPIO binary_sensor '%s': GPIO16 on ESP8266 doesn't support interrupts. "
            "Falling back to polling mode (same as in ESPHome <2025.7). "
            "The sensor will work exactly as before, but other pins have better "
            "performance with interrupts.",
            config.get(CONF_NAME, config[CONF_ID]),
        )
        use_interrupt = False

    # Check if pin is shared with other components (allow_other_uses)
    # When a pin is shared, interrupts can interfere with other components
    # (e.g., duty_cycle sensor) that need to monitor the pin's state changes.
    # Exception: deep_sleep wakeup pins are compatible with interrupts.
    if use_interrupt and config[CONF_PIN].get(CONF_ALLOW_OTHER_USES, False):
        pin_use_count = pins.PIN_SCHEMA_REGISTRY.get_count(
            CORE.target_platform, CORE.target_platform, pin_num
        )
        if not (_pin_is_deep_sleep_wakeup(pin_num) and pin_use_count == 2):
            _LOGGER.info(
                "GPIO binary_sensor '%s': Disabling interrupts because pin %s is shared with other components. "
                "The sensor will use polling mode for compatibility with other pin uses.",
                config.get(CONF_NAME, config[CONF_ID]),
                pin_num,
            )
            use_interrupt = False
        else:
            _LOGGER.debug(
                "GPIO binary_sensor '%s': Pin %s is shared with deep_sleep, "
                "keeping interrupts enabled.",
                config.get(CONF_NAME, config[CONF_ID]),
                pin_num,
            )
    if use_interrupt:
        cg.add(var.set_interrupt_type(config[CONF_INTERRUPT_TYPE]))
    else:
        # Only generate call when disabling interrupts (default is true)
        cg.add(var.set_use_interrupt(use_interrupt))
