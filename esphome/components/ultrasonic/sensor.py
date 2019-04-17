import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_ECHO_PIN, CONF_ID, CONF_NAME, CONF_TRIGGER_PIN, \
    CONF_UPDATE_INTERVAL, CONF_TIMEOUT, UNIT_METER, ICON_ARROW_EXPAND_VERTICAL

CONF_PULSE_TIME = 'pulse_time'

ultrasonic_ns = cg.esphome_ns.namespace('ultrasonic')
UltrasonicSensorComponent = ultrasonic_ns.class_('UltrasonicSensorComponent',
                                                 sensor.PollingSensorComponent)

CONFIG_SCHEMA = cv.nameable(
    sensor.sensor_schema(UNIT_METER, ICON_ARROW_EXPAND_VERTICAL, 2).extend({
        cv.GenerateID(): cv.declare_variable_id(UltrasonicSensorComponent),
        cv.Required(CONF_TRIGGER_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_ECHO_PIN): pins.internal_gpio_input_pin_schema,

        cv.Optional(CONF_TIMEOUT, default='2m'): cv.distance,
        cv.Optional(CONF_PULSE_TIME, default='10us'): cv.positive_time_period_microseconds,
        cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,

        cv.Optional('timeout_meter'): cv.invalid("The timeout_meter option has been renamed "
                                                 "to 'timeout' in 1.12."),
        cv.Optional('timeout_time'): cv.invalid("The timeout_time option has been removed. Please "
                                                "use 'timeout' in 1.12."),
    }))


def to_code(config):
    trigger = yield cg.gpio_pin_expression(config[CONF_TRIGGER_PIN])
    echo = yield cg.gpio_pin_expression(config[CONF_ECHO_PIN])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], trigger, echo,
                           config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    cg.add(var.set_timeout_us(config[CONF_TIMEOUT] / (0.000343 / 2)))
    cg.add(var.set_pulse_time_us(config[CONF_PULSE_TIME]))
