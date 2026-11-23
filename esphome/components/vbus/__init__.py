from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@ssieb"]
DOMAIN = "vbus"

DEPENDENCIES = ["uart"]

MULTI_CONF = True

vbus_ns = cg.esphome_ns.namespace("vbus")
VBus = vbus_ns.class_("VBus", uart.UARTDevice, cg.Component)

CONF_VBUS_ID = "vbus_id"
CONF_ON_DATAGRAM = "on_datagram"

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
        cv.Optional(CONF_ON_DATAGRAM): automation.validate_automation({}),
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
    var = await uart.new_uart_component(config)
    if datagram_automations := config.get(CONF_ON_DATAGRAM):
        cg.add_define("USE_VBUS_DATAGRAM")
        for conf in datagram_automations:
            await automation.build_callback_automation(
                var,
                "add_on_datagram_callback",
                [
                    (cg.uint16, "command"),
                    (cg.uint16, "source"),
                    (cg.uint16, "destination"),
                    (cg.uint16, "value_id"),
                    (cg.uint32, "value"),
                ],
                conf,
            )
