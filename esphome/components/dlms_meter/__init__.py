import re

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_PATTERN, CONF_PRIORITY

CODEOWNERS = ["@SimonFischer04", "@Tomer27cz", "@latonita", "@PolarGoose"]
DEPENDENCIES = ["uart"]

CONF_DLMS_METER_ID = "dlms_meter_id"
CONF_DECRYPTION_KEY = "decryption_key"
CONF_AUTH_KEY = "auth_key"
CONF_OBIS_CODE = "obis_code"
CONF_CUSTOM_PATTERNS = "custom_patterns"
CONF_SKIP_CRC = "skip_crc"
CONF_DEFAULT_OBIS = "default_obis"
CONF_PROVIDER = "provider"

dlms_meter_component_ns = cg.esphome_ns.namespace("dlms_meter")
DlmsMeterComponent = dlms_meter_component_ns.class_(
    "DlmsMeterComponent", cg.Component, uart.UARTDevice
)

# Maintain backwards compatibility mappings
NUMERIC_KEYS = {
    "voltage_l1": "1.0.32.7.0.255",
    "voltage_l2": "1.0.52.7.0.255",
    "voltage_l3": "1.0.72.7.0.255",
    "current_l1": "1.0.31.7.0.255",
    "current_l2": "1.0.51.7.0.255",
    "current_l3": "1.0.71.7.0.255",
    "active_power_plus": "1.0.1.7.0.255",
    "active_power_minus": "1.0.2.7.0.255",
    "active_energy_plus": "1.0.1.8.0.255",
    "active_energy_minus": "1.0.2.8.0.255",
    "reactive_energy_plus": "1.0.3.8.0.255",
    "reactive_energy_minus": "1.0.4.8.0.255",
    "power_factor": "1.0.13.7.0.255",
}

TEXT_KEYS = {
    "timestamp": "0.0.1.0.0.255",
    "meternumber": "0.0.96.1.0.255",
}


def validate_key(value):
    value = cv.string_strict(value)
    if len(value) != 32:
        raise cv.Invalid("Decryption key must be 32 hex characters (16 bytes)")
    try:
        return [int(value[i : i + 2], 16) for i in range(0, 32, 2)]
    except ValueError as exc:
        raise cv.Invalid("Decryption key must be hex values from 00 to FF") from exc


def obis_code(value):
    # Normalize the OBIS code to the strict A.B.C.D.E.F format
    bytes_list = parse_obis_code_bytes(value)
    return ".".join(str(b) for b in bytes_list)


def parse_obis_code_bytes(value):
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


def custom_pattern_dict(value):
    if isinstance(value, str):
        return {CONF_PATTERN: value}
    return value


def validate_custom_pattern(value):
    if CONF_DEFAULT_OBIS in value and CONF_NAME not in value:
        raise cv.Invalid(f"'{CONF_DEFAULT_OBIS}' requires '{CONF_NAME}' to be set")
    return value


CUSTOM_PATTERN_SCHEMA = cv.All(
    custom_pattern_dict,
    cv.Schema(
        {
            cv.Required(CONF_PATTERN): cv.string,
            cv.Optional(CONF_NAME): cv.string,
            cv.Optional(CONF_PRIORITY, default=0): cv.int_,
            cv.Optional(CONF_DEFAULT_OBIS): parse_obis_code_bytes,
        }
    ),
    validate_custom_pattern,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DlmsMeterComponent),
            cv.Optional(CONF_DECRYPTION_KEY): validate_key,
            cv.Optional(CONF_AUTH_KEY): validate_key,
            cv.Optional(CONF_CUSTOM_PATTERNS): cv.ensure_list(CUSTOM_PATTERN_SCHEMA),
            cv.Optional(CONF_SKIP_CRC, default=False): cv.boolean,
            cv.Optional(CONF_PROVIDER): cv.invalid(
                "The 'provider' option has been removed. The dlms_parser library now handles quirks dynamically. Please remove this option from your configuration."
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("dlms_meter", require_rx=True)


async def to_code(config):
    dec_key_expr = cg.RawExpression("std::nullopt")
    if CONF_DECRYPTION_KEY in config:
        dec_key_expr = cg.RawExpression(
            f"std::array<uint8_t, 16>{{{', '.join(str(x) for x in config[CONF_DECRYPTION_KEY])}}}"
        )

    auth_key_expr = cg.RawExpression("std::nullopt")
    if CONF_AUTH_KEY in config:
        auth_key_expr = cg.RawExpression(
            f"std::array<uint8_t, 16>{{{', '.join(str(x) for x in config[CONF_AUTH_KEY])}}}"
        )

    patterns = []
    if CONF_CUSTOM_PATTERNS in config:
        for p in config[CONF_CUSTOM_PATTERNS]:
            name_expr = (
                p[CONF_NAME] if CONF_NAME in p else cg.RawExpression("std::nullopt")
            )
            if CONF_DEFAULT_OBIS in p:
                obis_vals = p[CONF_DEFAULT_OBIS]
                obis_expr = cg.RawExpression(
                    f"std::array<uint8_t, 6>{{{obis_vals[0]}, {obis_vals[1]}, {obis_vals[2]}, {obis_vals[3]}, {obis_vals[4]}, {obis_vals[5]}}}"
                )
            else:
                obis_expr = cg.RawExpression("std::nullopt")

            patterns.append(
                cg.ArrayInitializer(
                    p[CONF_PATTERN],
                    name_expr,
                    p.get(CONF_PRIORITY, 0),
                    obis_expr,
                )
            )

    patterns_expr = (
        cg.ArrayInitializer(*patterns) if patterns else cg.RawExpression("{}")
    )

    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_SKIP_CRC],
        dec_key_expr,
        auth_key_expr,
        patterns_expr,
    )

    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add_library(
        "dlms_parser", "v1.0.0", "https://github.com/esphome-libs/dlms_parser"
    )
