from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_PIN
from esphome.core import coroutine_with_priority

heartbeat_led_ns = cg.esphome_ns.namespace('heartbeat_led')
HeartbeatLED = heartbeat_led_ns.class_('HeartbeatLED', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HeartbeatLED),
    cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(80.0)
def to_code(config):
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    rhs = HeartbeatLED.new(pin)
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield cg.register_component(var, config)
    cg.add(var.pre_setup())
    cg.add_define('USE_HEARTBEAT_LED')
