import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@ssieb"]

DEPENDENCIES = ["uart"]

MULTI_CONF = True

vbus_ns = cg.esphome_ns.namespace("vbus")
VBus = vbus_ns.class_("VBus", uart.UARTDevice, cg.Component)

CONF_VBUS_ID = "vbus_id"

CONF_DELTASOL_BS_PLUS = "deltasol_bs_plus"
CONF_DELTASOL_BS_2009 = "deltasol_bs_2009"
CONF_DELTASOL_BS2 = "deltasol_bs2"
CONF_DELTASOL_C = "deltasol_c"
CONF_DELTASOL_CS2 = "deltasol_cs2"
CONF_DELTASOL_CS4 = "deltasol_cs4"
CONF_DELTASOL_CS_PLUS = "deltasol_cs_plus"

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(VBus),
    }
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "vbus",
    baud_rate=9600,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
