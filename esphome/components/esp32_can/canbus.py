import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import canbus
from esphome.const import CONF_ID
from esphome.components.canbus import CONF_CAN_ID

CONF_RX_PIN = 'rx_pin'
CONF_TX_PIN = 'tx_pin'

esp32_can_ns = cg.esphome_ns.namespace('esp32_can')
esp32_can = esp32_can_ns.class_('ESP32Can', cg.Component, canbus.CanbusComponent)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(esp32_can),
    cv.Required(CONF_RX_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_TX_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_CAN_ID): cv.int_range(min=0, max=999),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = esp32_can.new()
    var = cg.Pvariable(config[CONF_ID], rhs)

    yield cg.register_component(var, config)
