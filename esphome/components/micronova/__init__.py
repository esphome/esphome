from dataclasses import dataclass

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority

CODEOWNERS = ["@jorre05", "@edenhaus"]

DEPENDENCIES = ["uart"]

DOMAIN = "micronova"


@dataclass
class MicronovaData:
    """Track micronova component state during code generation."""

    listener_count: int = 0
    has_writer: bool = False


def _get_data() -> MicronovaData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = MicronovaData()
    return CORE.data[DOMAIN]


CONF_MICRONOVA_ID = f"{DOMAIN}_id"
CONF_ENABLE_RX_PIN = "enable_rx_pin"
CONF_MEMORY_LOCATION = "memory_location"
CONF_MEMORY_ADDRESS = "memory_address"
DEFAULT_POLLING_INTERVAL = "60s"

micronova_ns = cg.esphome_ns.namespace(DOMAIN)

MicroNova = micronova_ns.class_("MicroNova", cg.Component, uart.UARTDevice)
MicroNovaListener = micronova_ns.class_("MicroNovaListener", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MicroNova),
        cv.Required(CONF_ENABLE_RX_PIN): pins.gpio_output_pin_schema,
    }
).extend(uart.UART_DEVICE_SCHEMA)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=1200,
    require_rx=True,
    require_tx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=2,
)


def MICRONOVA_ADDRESS_SCHEMA(
    *,
    default_memory_location: int | None = None,
    default_memory_address: int | None = None,
    is_polling_component: bool,
):
    location_key = (
        cv.Optional(CONF_MEMORY_LOCATION, default=default_memory_location)
        if default_memory_location is not None
        else cv.Required(CONF_MEMORY_LOCATION)
    )
    address_key = (
        cv.Optional(CONF_MEMORY_ADDRESS, default=default_memory_address)
        if default_memory_address is not None
        else cv.Required(CONF_MEMORY_ADDRESS)
    )
    schema = cv.Schema(
        {
            cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
            location_key: cv.hex_int_range(min=0x00, max=0x79),
            address_key: cv.hex_int_range(min=0x00, max=0xFF),
        }
    )
    if is_polling_component:
        schema = schema.extend(cv.polling_component_schema(DEFAULT_POLLING_INTERVAL))
    return schema


def register_micronova_writer() -> None:
    """Register a component that can write to the stove (button, switch, number)."""
    _get_data().has_writer = True


async def to_code_micronova_listener(mv, var, config):
    _get_data().listener_count += 1
    await cg.register_component(var, config)
    cg.add(var.set_memory_location(config[CONF_MEMORY_LOCATION]))
    cg.add(var.set_memory_address(config[CONF_MEMORY_ADDRESS]))
    # Register listener as last step as we need all properties set before registering
    cg.add(mv.register_micronova_listener(var))


async def to_code(config):
    enable_rx_pin = await cg.gpio_pin_expression(config[CONF_ENABLE_RX_PIN])
    var = cg.new_Pvariable(config[CONF_ID], enable_rx_pin)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    CORE.add_job(_final_step)


@coroutine_with_priority(CoroPriority.FINAL)
async def _final_step() -> None:
    """Add defines for listener and writer counts after all are registered."""
    data = _get_data()
    if data.listener_count == 0 and not data.has_writer:
        raise cv.Invalid(
            "No micronova entities configured. Add at least one micronova entity."
        )
    if data.listener_count > 255:
        raise cv.Invalid(
            f"Too many micronova reading entities ({data.listener_count}). Maximum is 255."
        )
    if data.listener_count > 0:
        cg.add_define("MICRONOVA_LISTENER_COUNT", data.listener_count)

    if data.has_writer:
        cg.add_define("USE_MICRONOVA_WRITER")
