import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import (
    CONF_COUNT_MODE,
    CONF_FALLING_EDGE,
    CONF_ID,
    CONF_INTERNAL_FILTER,
    CONF_PIN,
    CONF_RISING_EDGE,
    CONF_NUMBER,
    CONF_TOTAL,
    ICON_PULSE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_PULSES_PER_MINUTE,
    UNIT_PULSES,
)
from esphome.core import CORE

pulse_counter_ns = cg.esphome_ns.namespace("pulse_counter")
PulseCounterCountMode = pulse_counter_ns.enum("PulseCounterCountMode")
COUNT_MODES = {
    "DISABLE": PulseCounterCountMode.PULSE_COUNTER_DISABLE,
    "INCREMENT": PulseCounterCountMode.PULSE_COUNTER_INCREMENT,
    "DECREMENT": PulseCounterCountMode.PULSE_COUNTER_DECREMENT,
}

COUNT_MODE_SCHEMA = cv.enum(COUNT_MODES, upper=True)

PulseCounterSensor = pulse_counter_ns.class_(
    "PulseCounterSensor", sensor.Sensor, cg.PollingComponent
)


def validate_internal_filter(value):
    value = cv.positive_time_period_microseconds(value)
    if CORE.is_esp32:
        if value.total_microseconds > 13:
            raise cv.Invalid("Maximum internal filter value for ESP32 is 13us")
        return value

    return value


def validate_pulse_counter_pin(value):
    value = pins.internal_gpio_input_pin_schema(value)
    if CORE.is_esp8266 and value[CONF_NUMBER] >= 16:
        raise cv.Invalid(
            "Pins GPIO16 and GPIO17 cannot be used as pulse counters on ESP8266."
        )
    return value


def validate_count_mode(value):
    rising_edge = value[CONF_RISING_EDGE]
    falling_edge = value[CONF_FALLING_EDGE]
    if rising_edge == "DISABLE" and falling_edge == "DISABLE":
        raise cv.Invalid(
            "Can't set both count modes to DISABLE! This means no counting occurs at "
            "all!"
        )
    return value


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_PULSES_PER_MINUTE,
        icon=ICON_PULSE,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(PulseCounterSensor),
            cv.Required(CONF_PIN): validate_pulse_counter_pin,
            cv.Optional(
                CONF_COUNT_MODE,
                default={
                    CONF_RISING_EDGE: "INCREMENT",
                    CONF_FALLING_EDGE: "DISABLE",
                },
            ): cv.All(
                cv.Schema(
                    {
                        cv.Required(CONF_RISING_EDGE): COUNT_MODE_SCHEMA,
                        cv.Required(CONF_FALLING_EDGE): COUNT_MODE_SCHEMA,
                    }
                ),
                validate_count_mode,
            ),
            cv.Optional(CONF_INTERNAL_FILTER, default="13us"): validate_internal_filter,
            cv.Optional(CONF_TOTAL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PULSES,
                icon=ICON_PULSE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    count = config[CONF_COUNT_MODE]
    cg.add(var.set_rising_edge_mode(count[CONF_RISING_EDGE]))
    cg.add(var.set_falling_edge_mode(count[CONF_FALLING_EDGE]))
    cg.add(var.set_filter_us(config[CONF_INTERNAL_FILTER]))

    if CONF_TOTAL in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL])
        cg.add(var.set_total_sensor(sens))
