import esphome.codegen as cg
from esphome.components import event, uart
import esphome.config_validation as cv
from esphome.const import CONF_EVENT_TYPES
from esphome.types import ConfigType

from .. import uart_ns

CODEOWNERS = ["@eoasmxd"]

DEPENDENCIES = ["uart"]

UARTEvent = uart_ns.class_("UARTEvent", event.Event, uart.UARTDevice, cg.Component)
UARTEventMatcher = uart_ns.struct("UARTEventMatcher")


def validate_event_types(value) -> list[tuple[str, str | list[int]]]:
    if not isinstance(value, list):
        raise cv.Invalid("Event type must be a list of key-value mappings.")

    processed: list[tuple[str, str | list[int]]] = []
    for item in value:
        if not isinstance(item, dict):
            raise cv.Invalid(f"Event type item must be a mapping (dictionary): {item}")
        if len(item) != 1:
            raise cv.Invalid(
                f"Event type item must be a single key-value mapping: {item}"
            )

        # Get the single key-value pair
        event_name, match_data = next(iter(item.items()))

        if not isinstance(event_name, str):
            raise cv.Invalid(f"Event name (key) must be a string: {event_name}")

        try:
            # Try to validate as list of hex bytes
            match_data_bin = cv.ensure_list(cv.hex_uint8_t)(match_data)
            processed.append((event_name, match_data_bin))
            continue
        except cv.Invalid:
            pass  # Not binary, try string

        try:
            # Try to validate as string
            match_data_str = cv.string_strict(match_data)
            processed.append((event_name, match_data_str))
            continue
        except cv.Invalid:
            pass  # Not string either

        # If neither validation passed
        raise cv.Invalid(
            f"Event match data for '{event_name}' must be a string or a list of hex bytes. Invalid data: {match_data}"
        )

    if not processed:
        raise cv.Invalid("event_types must contain at least one event mapping.")
    if any(len(data) > 0xFFFF for _, data in processed):
        raise cv.Invalid("Event match data can be at most 65535 bytes.")

    return processed


CONFIG_SCHEMA = (
    event.event_schema(UARTEvent)
    .extend(
        {
            cv.Required(CONF_EVENT_TYPES): validate_event_types,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    event_names = [item[0] for item in config[CONF_EVENT_TYPES]]
    var = await event.new_event(config, event_types=event_names)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    matchers = []
    max_len = 0
    for event_name, match_data in config[CONF_EVENT_TYPES]:
        match_data = (
            [ord(c) for c in match_data]
            if isinstance(match_data, str)
            else [int(b) for b in match_data]
        )
        data = (
            cg.shared_progmem_array("uart_event_match", cg.uint8, match_data)
            if match_data
            else cg.nullptr
        )
        matchers.append(
            cg.StructInitializer(
                UARTEventMatcher,
                ("event_name", event_name),
                ("data", data),
                ("data_len", len(match_data)),
            )
        )
        max_len = max(max_len, len(match_data))
    table = cg.shared_progmem_array("uart_event_matchers", UARTEventMatcher, matchers)
    cg.add(var.set_matchers(table, len(matchers), max_len))
