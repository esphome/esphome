from dataclasses import dataclass

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@ljungqvist"]
DOMAIN = "ufm01"

MULTI_CONF = True

DEPENDENCIES = ["uart"]

ufm01_ns = cg.esphome_ns.namespace("ufm01")
UFM01Component = ufm01_ns.class_("UFM01Component", uart.UARTDevice, cg.Component)

CONF_UFM01_ID = "ufm01_id"


@dataclass
class UFM01Data:
    clear_action_used: bool = False


def _get_data() -> UFM01Data:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = UFM01Data()
    return CORE.data[DOMAIN]


def _note_clear_action(config: ConfigType) -> ConfigType:
    _get_data().clear_action_used = True
    return config


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UFM01Component),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

CLEAR_ACCUMULATED_FLOW_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(UFM01Component),
    },
    _note_clear_action,
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ufm01",
    require_tx=True,
    require_rx=True,
    baud_rate=2400,
    data_bits=8,
    parity="EVEN",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if _get_data().clear_action_used:
        cg.add_define("USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION")


automation.register_apply_action(
    "ufm01.clear_accumulated_flow",
    CLEAR_ACCUMULATED_FLOW_ACTION_SCHEMA,
    automation.ApplyCall("request_clear_accumulated_flow()"),
)
