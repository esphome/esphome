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
    if use_interrupt and CORE.is_esp8266 and config[CONF_PIN][CONF_NUMBER] == 16:
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
    # (e.g., duty_cycle sensor) that need to monitor the pin's state changes
    if use_interrupt and config[CONF_PIN].get(CONF_ALLOW_OTHER_USES, False):
        _LOGGER.info(
            "GPIO binary_sensor '%s': Disabling interrupts because pin %s is shared with other components. "
            "The sensor will use polling mode for compatibility with other pin uses.",
            config.get(CONF_NAME, config[CONF_ID]),
            config[CONF_PIN][CONF_NUMBER],
        )
        use_interrupt = False

    if use_interrupt:
        cg.add(var.set_interrupt_type(config[CONF_INTERRUPT_TYPE]))
    else:
        # Only generate call when disabling interrupts (default is true)
        cg.add(var.set_use_interrupt(use_interrupt))
