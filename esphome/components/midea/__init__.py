import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, remote_transmitter
from esphome.components.remote_base import CONF_TRANSMITTER_ID
from esphome.const import CONF_ID, CONF_PERIOD, CONF_NUM_ATTEMPTS, CONF_TIMEOUT

DEPENDENCIES = ["wifi", "uart"]
CODEOWNERS = ["@dudanov"]

midea_dongle_ns = cg.esphome_ns.namespace("midea_dongle")
MideaDongle = midea_dongle_ns.class_("MideaDongle", cg.Component, uart.UARTDevice)

CONF_MIDEA_DONGLE_ID = "midea_dongle_id"
CONF_STRENGTH_ICON = "strength_icon"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MideaDongle),
            cv.Optional(CONF_PERIOD, default="1s"): cv.time_period,
            cv.Optional(CONF_TIMEOUT, default="2s"): cv.time_period,
            cv.Optional(CONF_NUM_ATTEMPTS, default=3): cv.int_range(min=1, max=5),
            cv.Optional(CONF_TRANSMITTER_ID): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
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
    cg.add(var.set_period(config[CONF_PERIOD].total_milliseconds))
    cg.add(var.set_response_timeout(config[CONF_TIMEOUT].total_milliseconds))
    cg.add(var.set_request_attempts(config[CONF_NUM_ATTEMPTS]))
    if CONF_TRANSMITTER_ID in config:
        cg.add_define("USE_REMOTE_TRANSMITTER")
        transmitter_ = await cg.get_variable(config[CONF_TRANSMITTER_ID])
        cg.add(var.set_transmitter(transmitter_))
    cg.add_library("MideaControl", "1.0.0", "file://../MideaControl")
