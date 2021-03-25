import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import canbus
from esphome.const import CONF_ID, CONF_RX_PIN, CONF_TX_PIN, ESP_PLATFORM_ESP32
from esphome.components.canbus import CanbusComponent, CanSpeed, CONF_BIT_RATE

esp32_can_ns = cg.esphome_ns.namespace("esp32_can")
esp32_can = esp32_can_ns.class_("ESP32Can", CanbusComponent)

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

# Currently the driver only supports a subset of the bit rates defined in canbus
CAN_SPEEDS = {
    "50KBPS": CanSpeed.CAN_50KBPS,
    "100KBPS": CanSpeed.CAN_100KBPS,
    "125KBPS": CanSpeed.CAN_125KBPS,
    "250KBPS": CanSpeed.CAN_250KBPS,
    "500KBPS": CanSpeed.CAN_500KBPS,
    "1000KBPS": CanSpeed.CAN_1000KBPS,
}

CONFIG_SCHEMA = canbus.CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(esp32_can),
        cv.Optional(CONF_BIT_RATE, default="125KBPS"): cv.enum(CAN_SPEEDS, upper=True),
        cv.Required(CONF_RX_PIN): pins.input_pin,
        cv.Required(CONF_TX_PIN): pins.output_pin,
    }
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield canbus.register_canbus(var, config)

    rx = yield cg.gpio_pin_expression({'number': config[CONF_RX_PIN], 'mode': 'INPUT', 'inverted': False})
    cg.add(var.set_rx(rx))
    tx = yield cg.gpio_pin_expression({'number': config[CONF_TX_PIN], 'mode': 'OUTPUT', 'inverted': False})
    cg.add(var.set_tx(tx))
