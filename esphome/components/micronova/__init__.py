from dataclasses import dataclass
from typing import Any

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL, CONF_PLATFORM
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
import esphome.final_validate as fv

from .models import DEFAULT_MODEL, MODELS, get_model_defaults

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
        cv.Optional(CONF_MODEL): cv.one_of(*MODELS, lower=True, space="_"),
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


def MICRONOVA_ADDRESS_SCHEMA(*, is_polling_component: bool) -> cv.Schema:
    schema = cv.Schema(
        {
            cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
            cv.Optional(CONF_MEMORY_LOCATION): cv.hex_int_range(min=0x00, max=0x79),
            cv.Optional(CONF_MEMORY_ADDRESS): cv.hex_int_range(min=0x00, max=0xFF),
        }
    )
    if is_polling_component:
        schema = schema.extend(cv.polling_component_schema(DEFAULT_POLLING_INTERVAL))
    return schema


def register_micronova_writer() -> None:
    """Register a component that can write to the stove (button, switch, number)."""
    _get_data().has_writer = True


def final_validate_address(config: dict[str, Any]) -> dict[str, Any]:
    full_config = fv.full_config.get()
    micronova_path = full_config.get_path_for_id(config[CONF_MICRONOVA_ID])[:-1]
    micronova_config = full_config.get_config_for_path(micronova_path)
    model = micronova_config.get(CONF_MODEL, DEFAULT_MODEL)
    exclude_set = {CONF_PLATFORM, CONF_MICRONOVA_ID}

    for entity_key, value in config.items():
        if entity_key in exclude_set:
            continue

        default_memory = get_model_defaults(model, entity_key)

        for memory_key in (CONF_MEMORY_LOCATION, CONF_MEMORY_ADDRESS):
            if memory_key in value:
                continue

            if (
                default_memory
                and (
                    default_value := getattr(
                        default_memory, memory_key.removeprefix("memory_")
                    )
                )
                is not None
            ):
                value[memory_key] = default_value
            else:
                raise cv.Invalid(
                    f"'{memory_key}' is a required option for [{entity_key}].",
                    path=[entity_key, memory_key],
                )

    return config


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
