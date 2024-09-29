import os

from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_COUNT_MODE,
    CONF_DEBOUNCE,
    CONF_FALLING_EDGE,
    CONF_NUMBER,
    CONF_PIN,
    CONF_RISING_EDGE,
    CONF_SLEEP_DURATION,
    ICON_PULSE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PULSES_PER_MINUTE,
)
from esphome.core import CORE

pulse_counter_ulp_ns = cg.esphome_ns.namespace("pulse_counter_ulp")
CountMode = pulse_counter_ulp_ns.enum("CountMode", is_class=True)
COUNT_MODES = {
    "DISABLE": CountMode.disable,
    "INCREMENT": CountMode.increment,
    "DECREMENT": CountMode.decrement,
}

COUNT_MODE_SCHEMA = cv.enum(COUNT_MODES, upper=True)

PulseCounterUlpSensor = pulse_counter_ulp_ns.class_(
    "PulseCounterUlpSensor", sensor.Sensor, cg.PollingComponent
)


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


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        PulseCounterUlpSensor,
        unit_of_measurement=UNIT_PULSES_PER_MINUTE,
        icon=ICON_PULSE,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
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
            cv.Optional(
                CONF_SLEEP_DURATION, default="20000us"
            ): cv.positive_time_period_microseconds,
            cv.Optional(CONF_DEBOUNCE, default=3): cv.positive_int,
        },
    )
    .extend(cv.polling_component_schema("60s")),
)


async def to_code(config):
    esp32.add_extra_build_file(
        "src/CMakeLists.txt",
        os.path.join(os.path.dirname(__file__), "CMakeLists.txt"),
    )
    esp32.add_extra_build_file(
        "ulp/pulse_cnt.S",
        os.path.join(os.path.dirname(__file__), "ulp/pulse_cnt.S"),
    )
    esp32.add_idf_sdkconfig_option("CONFIG_ULP_COPROC_ENABLED", True)
    esp32.add_idf_sdkconfig_option("CONFIG_ULP_COPROC_TYPE_FSM", True)
    esp32.add_idf_sdkconfig_option("CONFIG_ULP_COPROC_RESERVE_MEM", 1024)
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    count = config[CONF_COUNT_MODE]
    cg.add(var.set_rising_edge_mode(count[CONF_RISING_EDGE]))
    cg.add(var.set_falling_edge_mode(count[CONF_FALLING_EDGE]))
    cg.add(var.set_sleep_duration(config[CONF_SLEEP_DURATION]))
    cg.add(var.set_debounce(config[CONF_DEBOUNCE]))
