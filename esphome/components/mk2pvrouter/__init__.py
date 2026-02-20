from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, CoroPriority, coroutine_with_priority

CODEOWNERS = ["@FredM67"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

DOMAIN = "mk2pvrouter"

mk2pvrouter_ns = cg.esphome_ns.namespace("mk2pvrouter")
Mk2PVRouter = mk2pvrouter_ns.class_("Mk2PVRouter", cg.PollingComponent, uart.UARTDevice)

CONF_MK2PVROUTER_ID = "mk2pvrouter_id"
CONF_TAG_NAME = "tag_name"


@dataclass
class Mk2PVRouterData:
    listener_count: int = 0


def _get_data() -> Mk2PVRouterData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = Mk2PVRouterData()
    return CORE.data[DOMAIN]


def register_listener() -> None:
    """Track listener registration for StaticVector sizing."""
    _get_data().listener_count += 1


MK2PVROUTER_LISTENER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MK2PVROUTER_ID): cv.use_id(Mk2PVRouter),
        cv.Required(CONF_TAG_NAME): cv.All(cv.string, lambda x: x.upper()),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Mk2PVRouter),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "mk2pvrouter", baud_rate=9600, parity="EVEN", data_bits=7, require_rx=True
)


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_listener_count_define() -> None:
    listener_count = _get_data().listener_count
    if listener_count > 0:
        cg.add_define("MK2PVROUTER_MAX_LISTENERS", listener_count)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    CORE.add_job(_add_listener_count_define)
