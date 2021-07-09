import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, remote_transmitter
from esphome.components.remote_base import CONF_TRANSMITTER_ID
from esphome.const import CONF_ID

DEPENDENCIES = ["wifi", "uart"]
AUTO_LOAD = ["remote_transmitter"]
CODEOWNERS = ["@dudanov"]

midea_dongle_ns = cg.esphome_ns.namespace("midea_dongle")
MideaDongle = midea_dongle_ns.class_("MideaDongle", cg.Component, uart.UARTDevice)

CONF_MIDEA_DONGLE_ID = "midea_dongle_id"
CONF_STRENGTH_ICON = "strength_icon"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MideaDongle),
            cv.Optional(CONF_STRENGTH_ICON, default=False): cv.boolean,
            cv.Optional(CONF_TRANSMITTER_ID): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
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
    if CONF_TRANSMITTER_ID in config:
        transmitter_ = await cg.get_variable(config[CONF_TRANSMITTER_ID])
        cg.add(var.set_transmitter(transmitter_))
