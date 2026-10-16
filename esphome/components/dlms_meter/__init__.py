import re
from typing import Any

import esphome.codegen as cg
from esphome.components import esp32, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_PATTERN,
    CONF_PRIORITY,
    CONF_RECEIVE_TIMEOUT,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@SimonFischer04", "@Tomer27cz", "@latonita", "@PolarGoose"]
DEPENDENCIES = ["uart"]
DOMAIN = "dlms_meter"

CONF_DLMS_METER_ID = "dlms_meter_id"
CONF_DECRYPTION_KEY = "decryption_key"
CONF_AUTH_KEY = "auth_key"
CONF_OBIS_CODE = "obis_code"
CONF_CUSTOM_PATTERNS = "custom_patterns"
CONF_SKIP_CRC = "skip_crc"
CONF_DEFAULT_OBIS = "default_obis"
CONF_PROVIDER = "provider"

DLMS_PARSER_VERSION = "2.2.0"

dlms_meter_component_ns = cg.esphome_ns.namespace("dlms_meter")
DlmsMeterComponent = dlms_meter_component_ns.class_(
    "DlmsMeterComponent", cg.Component, uart.UARTDevice
)
CustomPattern = dlms_meter_component_ns.struct("CustomPattern")
ObisId = cg.global_ns.namespace("dlms_parser").class_("ObisId")


def obis_string_to_byte_list(value: Any) -> list[int]:
    value = cv.string(value)
    normalized = re.sub(r"[\-\:\*]", ".", value)
    parts = normalized.split(".")
    if len(parts) < 5 or len(parts) > 6:
        raise cv.Invalid("OBIS code must have 5 or 6 parts")
    try:
        bytes_list = [int(p) for p in parts]
    except ValueError as exc:
        raise cv.Invalid("OBIS code parts must be integers") from exc
    for b in bytes_list:
        if b < 0 or b > 255:
            raise cv.Invalid("OBIS code parts must be between 0 and 255")
    if len(bytes_list) == 5:
        bytes_list.append(255)
    return bytes_list


def to_obis_id_struct(value: list[int]) -> cg.Expression:
    return ObisId(*value)


_request_sensor_slot = cg.slot_counter("DLMS_MAX_SENSORS")
_request_text_sensor_slot = cg.slot_counter("DLMS_MAX_TEXT_SENSORS")
_request_binary_sensor_slot = cg.slot_counter("DLMS_MAX_BINARY_SENSORS")


def register_sensor(hub: MockObj, obis: list[int], var: MockObj) -> None:
    _request_sensor_slot(str(hub))
    cg.add(hub.register_sensor(to_obis_id_struct(obis), var))


def register_text_sensor(hub: MockObj, obis: list[int], var: MockObj) -> None:
    _request_text_sensor_slot(str(hub))
    cg.add(hub.register_text_sensor(to_obis_id_struct(obis), var))


def register_binary_sensor(hub: MockObj, obis: list[int], var: MockObj) -> None:
    _request_binary_sensor_slot(str(hub))
    cg.add(hub.register_binary_sensor(to_obis_id_struct(obis), var))


def custom_pattern_dict(value: Any) -> ConfigType:
    if isinstance(value, str):
        return {CONF_PATTERN: value}
    return value


CUSTOM_PATTERN_SCHEMA = cv.All(
    custom_pattern_dict,
    cv.Schema(
        {
            cv.Required(CONF_PATTERN): cv.string,
            cv.Optional(CONF_NAME, default="CUSTOM"): cv.string,
            cv.Optional(CONF_PRIORITY, default=0): cv.int_,
            cv.Optional(
                CONF_DEFAULT_OBIS, default="0.0.0.0.0.0"
            ): obis_string_to_byte_list,
        }
    ),
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DlmsMeterComponent),
            cv.Optional(CONF_DECRYPTION_KEY): cv.bind_key(name="Decryption key"),
            cv.Optional(CONF_AUTH_KEY): cv.bind_key(name="Authentication key"),
            cv.Optional(CONF_CUSTOM_PATTERNS): cv.ensure_list(CUSTOM_PATTERN_SCHEMA),
            cv.Optional(CONF_SKIP_CRC, default=False): cv.boolean,
            # Removed in 2026.11.0 - kept to provide helpful error message
            # Remove before 2027.5.0
            cv.Optional(CONF_PROVIDER): cv.invalid(
                "The 'provider' option has been removed in ESPHome 2026.11.0.\n"
                "For 'provider: netznoe', replace it with:\n\n"
                "custom_patterns:\n"
                '  - pattern: "L, TSTR"\n'
                '    name: "MeterID"\n'
                '    default_obis: "0.0.96.1.0.255"\n'
                '  - pattern: "F, TDTM"\n'
                '    name: "DateTime"\n'
                '    default_obis: "0.0.1.0.0.255"\n\n'
                "For any other provider, remove the option"
            ),
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default="1000ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("dlms_meter", require_rx=True)


async def to_code(config: ConfigType) -> None:
    custom_patterns = [
        cg.StructInitializer(
            CustomPattern,
            (CONF_PATTERN, pattern[CONF_PATTERN]),
            (CONF_NAME, pattern[CONF_NAME]),
            (CONF_PRIORITY, pattern[CONF_PRIORITY]),
            (CONF_DEFAULT_OBIS, to_obis_id_struct(pattern[CONF_DEFAULT_OBIS])),
        )
        for pattern in config.get(CONF_CUSTOM_PATTERNS, [])
    ]

    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_RECEIVE_TIMEOUT],
        config[CONF_SKIP_CRC],
        config.get(CONF_DECRYPTION_KEY, cg.nullptr),
        config.get(CONF_AUTH_KEY, cg.nullptr),
        cg.ArrayInitializer(*custom_patterns, multiline=True),
    )

    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CORE.is_esp32:
        esp32.add_idf_component(name="esphome/dlms_parser", ref=DLMS_PARSER_VERSION)
    else:
        cg.add_library("esphome/dlms_parser", DLMS_PARSER_VERSION)
