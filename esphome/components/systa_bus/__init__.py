from collections.abc import Callable
import logging

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SETUP_PRIORITY
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@Mat931"]
DOMAIN = "systa_bus"

DEPENDENCIES = ["uart"]

MULTI_CONF = True

systa_bus_ns = cg.esphome_ns.namespace("systa_bus")
SystaBus = systa_bus_ns.class_("SystaBus", cg.Component, uart.UARTDevice)

CONF_SYSTA_BUS_ID = "systa_bus_id"

CONF_SYSTASOLAR_AQUA = "systasolar_aqua"

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SystaBus),
    }
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "systa_bus", baud_rate=9600, require_rx=True
)

# Children publish from each SystaSolar Aqua sensor data frame
AQUA_PUBLISHER = cg.HubFunction(
    "aqua",
    cg.void,
    [(cg.std_span.template(cg.uint8.operator("const")), "message")],
    define="USE_SYSTA_BUS_AQUA_PUBLISHER",
)
MESSAGE = AQUA_PUBLISHER.param("message")
# Children's dump_config lines, logged under the bus's tag
DUMP_PUBLISHER = cg.HubFunction(
    "dump",
    cg.void,
    [(cg.const_char_ptr, "tag")],
)
DUMP_TAG = DUMP_PUBLISHER.param("tag")


def _warn_ignored_keys(platform: str) -> Callable[[ConfigType], ConfigType]:
    # Remove before 2027.5.0
    def validator(config: ConfigType) -> ConfigType:
        for key in (CONF_ID, CONF_SETUP_PRIORITY):
            if key in config:
                _LOGGER.warning(
                    "[systa_bus] '%s' on the %s platform has no effect and is deprecated. "
                    "Will be removed in 2027.5.0",
                    key,
                    platform,
                )
        return config

    return validator


def child_schema(platform: str, schema: dict) -> cv.All:
    """A model schema for a child platform; old configs may still set id and setup_priority."""
    return cv.All(
        cv.Schema(
            {
                cv.GenerateID(CONF_SYSTA_BUS_ID): cv.use_id(SystaBus),
                # Remove before 2027.5.0
                cv.Optional(CONF_ID): cv.All(cv.string, cv.validate_id_name),
                cv.Optional(CONF_SETUP_PRIORITY): cv.float_,
                **schema,
            }
        ),
        _warn_ignored_keys(platform),
    )


async def to_code(config: ConfigType) -> None:
    await uart.new_uart_component(config)
