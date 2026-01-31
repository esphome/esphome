import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ECHO_PIN,
    CONF_TIMEOUT,
    CONF_TRIGGER_PIN,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)

_LOGGER = logging.getLogger(__name__)

CONF_PULSE_TIME = "pulse_time"

ultrasonic_ns = cg.esphome_ns.namespace("ultrasonic")
UltrasonicSensorComponent = ultrasonic_ns.class_(
    "UltrasonicSensorComponent", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        UltrasonicSensorComponent,
        unit_of_measurement=UNIT_METER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_TRIGGER_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_ECHO_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_TIMEOUT): cv.distance,
            cv.Optional(
                CONF_PULSE_TIME, default="10us"
            ): cv.positive_time_period_microseconds,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    trigger = await cg.gpio_pin_expression(config[CONF_TRIGGER_PIN])
    cg.add(var.set_trigger_pin(trigger))
    echo = await cg.gpio_pin_expression(config[CONF_ECHO_PIN])
    cg.add(var.set_echo_pin(echo))

    # Remove before 2026.8.0
    if CONF_TIMEOUT in config:
        _LOGGER.warning(
            "'timeout' option is deprecated and will be removed in 2026.8.0. "
            "The option has no effect and can be safely removed."
        )

    cg.add(var.set_pulse_time_us(config[CONF_PULSE_TIME]))
