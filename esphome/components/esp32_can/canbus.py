import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import canbus
from esphome.const import CONF_ID
from esphome.components.canbus import CanbusComponent

CONF_RX_PIN = 'rx_pin'
CONF_TX_PIN = 'tx_pin'

esp32_can_ns = cg.esphome_ns.namespace('esp32_can')
esp32_can = esp32_can_ns.class_('ESP32Can', CanbusComponent)

CONFIG_SCHEMA = canbus.CONFIG_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(esp32_can),
    cv.Required(CONF_RX_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_TX_PIN): pins.gpio_output_pin_schema,
})


def to_code(config):
    rhs = esp32_can.new()
    var = cg.new_Pvariable(config[CONF_ID])
    yield canbus.register_canbus(var, config)
