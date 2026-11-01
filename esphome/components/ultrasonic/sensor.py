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
)
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CONF_PULSE_TIME = "pulse_time"

ultrasonic_ns = cg.esphome_ns.namespace("ultrasonic")
UltrasonicSensorComponent = ultrasonic_ns.class_(
    "UltrasonicSensorComponent", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        UltrasonicSensorComponent,
        kind=sensor.LENGTH_M,
        icon=ICON_ARROW_EXPAND_VERTICAL,
    )
    .extend(
        {
            cv.Required(CONF_TRIGGER_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_ECHO_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_TIMEOUT, default="2m"): cv.distance,
            cv.Optional(
                CONF_PULSE_TIME, default="10us"
            ): cv.positive_time_period_microseconds,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    await cg.add_pin(var.set_trigger_pin, config[CONF_TRIGGER_PIN])
    await cg.add_pin(var.set_echo_pin, config[CONF_ECHO_PIN])
    cg.add(var.set_timeout_us(config[CONF_TIMEOUT] / (0.000343 / 2)))
    cg.add(var.set_pulse_time_us(config[CONF_PULSE_TIME]))
