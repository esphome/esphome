from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PLATFORM
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@FredM67"]
DEPENDENCIES = ["uart"]

mk2pvrouter_ns = cg.esphome_ns.namespace("mk2pvrouter")
Mk2PVRouter = mk2pvrouter_ns.class_("Mk2PVRouter", cg.Component, uart.UARTDevice)

CONF_MK2PVROUTER_ID = "mk2pvrouter_id"
CONF_TAG_NAME = "tag_name"

DOMAIN = "mk2pvrouter"

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
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


@dataclass
class Mk2PVRouterData:
    listener_count: int = 0


def _get_data() -> Mk2PVRouterData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = Mk2PVRouterData()
    return CORE.data[DOMAIN]


def final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()

    # Count listeners (IDs are resolved at final_validate stage)
    _get_data().listener_count = sum(
        1
        for platform_key in ("sensor", "binary_sensor", "text_sensor")
        for entry in full_config.get(platform_key, [])
        if entry.get(CONF_PLATFORM) == "mk2pvrouter"
    )

    # Validate UART settings
    schema = uart.final_validate_device_schema(
        "mk2pvrouter", baud_rate=9600, parity="EVEN", data_bits=7, require_rx=True
    )
    return schema(config)


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Initialize listener storage with exact count from final_validate
    listener_count = _get_data().listener_count
    if listener_count > 0:
        cg.add(var.init_listeners(listener_count))
