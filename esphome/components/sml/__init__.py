import re

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_DATA

CODEOWNERS = ["@alengwenus"]

DEPENDENCIES = ["uart"]

sml_ns = cg.esphome_ns.namespace("sml")
Sml = sml_ns.class_("Sml", cg.Component, uart.UARTDevice)
MULTI_CONF = True

CONF_SML_ID = "sml_id"
CONF_OBIS_CODE = "obis_code"
CONF_SERVER_ID = "server_id"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Sml),
            cv.Optional(CONF_ON_DATA): automation.validate_automation({}),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_DATA,
        "add_on_data_callback",
        [
            (
                cg.std_vector.template(cg.uint8).operator("ref").operator("const"),
                "bytes",
            ),
            (cg.bool_, "valid"),
        ],
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


def obis_code(value):
    value = cv.string(value)
    match = re.match(r"^\d{1,3}-\d{1,3}:\d{1,3}\.\d{1,3}\.\d{1,3}$", value)
    if match is None:
        raise cv.Invalid(f"{value} is not a valid OBIS code")
    return value
