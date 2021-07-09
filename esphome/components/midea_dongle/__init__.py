import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import remote_transmitter, uart
from esphome.const import CONF_ID
from esphome import pins

DEPENDENCIES = ["wifi", "uart"]
CODEOWNERS = ["@dudanov"]

midea_dongle_ns = cg.esphome_ns.namespace("midea_dongle")
MideaDongle = midea_dongle_ns.class_("MideaDongle", cg.Component, uart.UARTDevice)

CONF_MIDEA_DONGLE_ID = "midea_dongle_id"
CONF_IR_TRANSMIT_PIN = "ir_transmit_pin"
CONF_STRENGTH_ICON = "strength_icon"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MideaDongle),
            cv.Optional(CONF_IR_TRANSMIT_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_STRENGTH_ICON, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.use_strength_icon(config[CONF_STRENGTH_ICON]))
    if CONF_IR_TRANSMIT_PIN in config:
        pin_ = await cg.gpio_pin_expression(config[CONF_IR_TRANSMIT_PIN])
        transmitter_ = remote_transmitter.RemoteTransmitterComponent.new(pin_)
        cg.add(var.set_transmitter(transmitter_))
