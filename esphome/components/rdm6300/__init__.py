from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ON_TAG
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor"]
DOMAIN = "rdm6300"
MULTI_CONF = True

rdm6300_ns = cg.esphome_ns.namespace("rdm6300")
RDM6300Component = rdm6300_ns.class_("RDM6300Component", cg.Component, uart.UARTDevice)

_AUTOMATION_LISTS = (
    automation.AutomationList(
        CONF_ON_TAG, "add_on_tag_automation", "RDM6300_ON_TAG_COUNT", [(cg.uint32, "x")]
    ),
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RDM6300Component),
            cv.Optional(CONF_ON_TAG): automation.validate_automation(),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "rdm6300", baud_rate=9600, require_rx=True
)


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)
    await automation.build_automation_lists(var, config, _AUTOMATION_LISTS)
