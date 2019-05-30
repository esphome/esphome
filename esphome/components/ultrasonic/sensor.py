import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_ECHO_PIN, CONF_ID, CONF_TRIGGER_PIN, \
    CONF_TIMEOUT, UNIT_METER, ICON_ARROW_EXPAND_VERTICAL

CONF_PULSE_TIME = 'pulse_time'

ultrasonic_ns = cg.esphome_ns.namespace('ultrasonic')
UltrasonicSensorComponent = ultrasonic_ns.class_('UltrasonicSensorComponent',
                                                 sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_METER, ICON_ARROW_EXPAND_VERTICAL, 2).extend({
    cv.GenerateID(): cv.declare_id(UltrasonicSensorComponent),
    cv.Required(CONF_TRIGGER_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_ECHO_PIN): pins.internal_gpio_input_pin_schema,

    cv.Optional(CONF_TIMEOUT, default='2m'): cv.distance,
    cv.Optional(CONF_PULSE_TIME, default='10us'): cv.positive_time_period_microseconds,

    cv.Optional('timeout_meter'): cv.invalid("The timeout_meter option has been renamed "
                                             "to 'timeout' in 1.12."),
    cv.Optional('timeout_time'): cv.invalid("The timeout_time option has been removed. Please "
                                            "use 'timeout' in 1.12."),
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    trigger = yield cg.gpio_pin_expression(config[CONF_TRIGGER_PIN])
    cg.add(var.set_trigger_pin(trigger))
    echo = yield cg.gpio_pin_expression(config[CONF_ECHO_PIN])
    cg.add(var.set_echo_pin(echo))

    cg.add(var.set_timeout_us(config[CONF_TIMEOUT] / (0.000343 / 2)))
    cg.add(var.set_pulse_time_us(config[CONF_PULSE_TIME]))
