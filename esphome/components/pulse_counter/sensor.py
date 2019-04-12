from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_COUNT_MODE, CONF_FALLING_EDGE, CONF_ID, CONF_INTERNAL_FILTER, \
    CONF_NAME, CONF_PIN, CONF_RISING_EDGE, CONF_UPDATE_INTERVAL, CONF_NUMBER, \
    CONF_ACCURACY_DECIMALS, CONF_ICON, CONF_UNIT_OF_MEASUREMENT, ICON_PULSE, UNIT_PULSES_PER_MINUTE
from esphome.core import CORE

pulse_counter_ns = cg.esphome_ns.namespace('pulse_counter')
PulseCounterCountMode = pulse_counter_ns.enum('PulseCounterCountMode')
COUNT_MODES = {
    'DISABLE': PulseCounterCountMode.PULSE_COUNTER_DISABLE,
    'INCREMENT': PulseCounterCountMode.PULSE_COUNTER_INCREMENT,
    'DECREMENT': PulseCounterCountMode.PULSE_COUNTER_DECREMENT,
}

COUNT_MODE_SCHEMA = cv.one_of(*COUNT_MODES, upper=True)

PulseCounterSensor = pulse_counter_ns.class_('PulseCounterSensor',
                                             sensor.PollingSensorComponent)


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
        raise cv.Invalid("Pins GPIO16 and GPIO17 cannot be used as pulse counters on ESP8266.")
    return value


PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(PulseCounterSensor),
    cv.Required(CONF_PIN): validate_pulse_counter_pin,
    cv.Optional(CONF_COUNT_MODE, default={
        CONF_RISING_EDGE: 'INCREMENT',
        CONF_FALLING_EDGE: 'DISABLE',
    }): cv.Schema({
        cv.Required(CONF_RISING_EDGE): COUNT_MODE_SCHEMA,
        cv.Required(CONF_FALLING_EDGE): COUNT_MODE_SCHEMA,
    }),
    cv.Optional(CONF_INTERNAL_FILTER, default='13us'): validate_internal_filter,
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,

    cv.Optional(CONF_ACCURACY_DECIMALS, default=2): sensor.accuracy_decimals,
    cv.Optional(CONF_ICON, default=ICON_PULSE): sensor.icon,
    cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_PULSES_PER_MINUTE):
        sensor.unit_of_measurement,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    count = config[CONF_COUNT_MODE]
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], pin, config[CONF_UPDATE_INTERVAL],
                           COUNT_MODES[count[CONF_RISING_EDGE]],
                           COUNT_MODES[count[CONF_FALLING_EDGE]],
                           config[CONF_INTERNAL_FILTER])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
