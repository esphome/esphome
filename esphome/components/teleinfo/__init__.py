import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = ["@0hax"]

teleinfo_ns = cg.esphome_ns.namespace("teleinfo")
TeleInfo = teleinfo_ns.class_("TeleInfo", cg.PollingComponent, uart.UARTDevice)
CONF_TELEINFO_ID = "teleinfo_id"

CONF_HISTORICAL_MODE = "historical_mode"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TeleInfo),
            cv.Optional(CONF_HISTORICAL_MODE, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_HISTORICAL_MODE])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
