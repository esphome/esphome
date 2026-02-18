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

    use_interrupt = config[CONF_USE_INTERRUPT]

    if use_interrupt and CORE.is_esp8266 and config[CONF_PIN][CONF_NUMBER] == 16:
        _LOGGER.warning(
            "GPIO binary_sensor '%s': GPIO16 on ESP8266 doesn't support interrupts. "
            "Falling back to polling mode.",
            config.get(CONF_NAME, config[CONF_ID]),
        )
        use_interrupt = False

    if use_interrupt and config[CONF_PIN].get(CONF_ALLOW_OTHER_USES, False):
        is_deep_sleep_pin = False

        if CORE.config and "deep_sleep" in CORE.config:
            deep_sleep_config = CORE.config.get("deep_sleep")
            if deep_sleep_config:
                if "wakeup_pin" in deep_sleep_config:
                    ds_pin = deep_sleep_config["wakeup_pin"][CONF_NUMBER]
                    if ds_pin == config[CONF_PIN][CONF_NUMBER]:
                        is_deep_sleep_pin = True

                elif "esp32_ext1_wakeup" in deep_sleep_config:
                    ext1_config = deep_sleep_config["esp32_ext1_wakeup"]
                    if "pins" in ext1_config:
                        for pin_config in ext1_config["pins"]:
                            if pin_config.get(CONF_NUMBER) == config[CONF_PIN][CONF_NUMBER]:
                                is_deep_sleep_pin = True
                                break

        if not is_deep_sleep_pin:
            _LOGGER.info(
                "GPIO binary_sensor '%s': Disabling interrupts because pin %s is shared with other components. "
                "The sensor will use polling mode.",
                config.get(CONF_NAME, config[CONF_ID]),
                config[CONF_PIN][CONF_NUMBER],
            )
            use_interrupt = False

    if use_interrupt:
        cg.add(var.set_interrupt_type(config[CONF_INTERRUPT_TYPE]))
    else:
        cg.add(var.set_use_interrupt(use_interrupt))