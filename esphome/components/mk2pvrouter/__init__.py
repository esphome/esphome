import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_TAG
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@FredM67"]
DEPENDENCIES = ["uart"]
DOMAIN = "mk2pvrouter"

mk2pvrouter_ns = cg.esphome_ns.namespace("mk2pvrouter")
Mk2PVRouter = mk2pvrouter_ns.class_("Mk2PVRouter", cg.Component, uart.UARTDevice)

CONF_MK2PVROUTER_ID = "mk2pvrouter_id"

# Tags are copied into a fixed-size buffer (MAX_TAG_SIZE = 8 in mk2pvrouter.h),
# which needs room for a trailing null terminator.
MAX_TAG_LEN = 7

MK2PVROUTER_LISTENER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MK2PVROUTER_ID): cv.use_id(Mk2PVRouter),
        cv.Required(CONF_TAG): cv.All(
            cv.string_strict, cv.Length(min=1, max=MAX_TAG_LEN), lambda x: x.upper()
        ),
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


def final_validate(config: ConfigType) -> None:
    # Validate UART settings
    schema = uart.final_validate_device_schema(
        "mk2pvrouter",
        baud_rate=9600,
        parity="EVEN",
        data_bits=7,
        stop_bits=1,
        require_rx=True,
        require_tx=False,
    )
    schema(config)


FINAL_VALIDATE_SCHEMA = final_validate


_register_mk2pvrouter_listener = cg.slot_list(
    "MK2PVROUTER_LISTENER_COUNT", "register_mk2pvrouter_listener"
)


async def register_mk2pvrouter_listener(mk2pvrouter: MockObj, var: MockObj) -> None:
    """Register a listener with its hub."""
    cg.add(_register_mk2pvrouter_listener(mk2pvrouter, var))


async def to_code(config: ConfigType) -> None:
    await uart.new_uart_component(config)
