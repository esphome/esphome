import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_RX_PIN, CONF_TX_PIN
from esphome.core import CORE
from esphome.components import canbus
from esphome.components.canbus import CanbusComponent


CODEOWNERS = ["@michaelansel", "@cbialobos"]

espCanBus_ns = cg.esphome_ns.namespace("esp32_canbus")
espCanBus = espCanBus_ns.class_("EspCanBus", CanbusComponent, cg.Component)

CONFIG_SCHEMA = canbus.CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(espCanBus),
        cv.Required(CONF_RX_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_TX_PIN): pins.gpio_output_pin_schema,
    }
)


async def to_code(config):
    rhs = espCanBus.new()
    var = cg.Pvariable(config[CONF_ID], rhs)
    await canbus.register_canbus(var, config)
    rx_pin = await cg.gpio_pin_expression(config[CONF_RX_PIN])
    cg.add(var.set_rx_pin(rx_pin))
    tx_pin = await cg.gpio_pin_expression(config[CONF_TX_PIN])
    cg.add(var.set_tx_pin(tx_pin))
    if CORE.is_esp32:
        cg.add_library("miwagner/ESP32CAN", None)
