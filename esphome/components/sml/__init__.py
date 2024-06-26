import re

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@alengwenus"]

DEPENDENCIES = ["uart"]

sml_ns = cg.esphome_ns.namespace("sml")
Sml = sml_ns.class_("Sml", cg.Component, uart.UARTDevice)
MULTI_CONF = True

CONF_SML_ID = "sml_id"
CONF_OBIS_CODE = "obis_code"
CONF_SERVER_ID = "server_id"
CONF_ON_DATA = "on_data"

sml_ns = cg.esphome_ns.namespace("sml")

DataTrigger = sml_ns.class_(
    "DataTrigger",
    automation.Trigger.template(
        cg.std_vector.template(cg.uint8).operator("ref"), cg.bool_
    ),
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Sml),
        cv.Optional(CONF_ON_DATA): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DataTrigger),
            }
        ),
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    for conf in config.get(CONF_ON_DATA, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (
                    cg.std_vector.template(cg.uint8).operator("ref").operator("const"),
                    "bytes",
                ),
                (cg.bool_, "valid"),
            ],
            conf,
        )


def obis_code(value):
    value = cv.string(value)
    match = re.match(r"^\d{1,3}-\d{1,3}:\d{1,3}\.\d{1,3}\.\d{1,3}$", value)
    if match is None:
        raise cv.Invalid(f"{value} is not a valid OBIS code")
    return value
