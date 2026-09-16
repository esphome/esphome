import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@s1lvi0"]
MULTI_CONF = True
DEPENDENCIES = ["uart"]

CONF_BMS_DALY_ID = "bms_daly_id"

daly_bms = cg.esphome_ns.namespace("daly_bms")
DalyBmsComponent = daly_bms.class_(
    "DalyBmsComponent", cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DalyBmsComponent),
            cv.Optional(CONF_ADDRESS, default=0x80): cv.positive_int,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("30s"))
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "daly_bms",
    baud_rate=9600,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_address(config[CONF_ADDRESS]))
