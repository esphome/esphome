import logging
import re

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

_LOGGER = logging.getLogger(__name__)

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


def validate_provider_deprecation(config):
    if CONF_PROVIDER in config:
        provider = str(config[CONF_PROVIDER]).lower()
        if provider == "netznoe":
            _LOGGER.warning(
                "The 'provider: netznoe' option is deprecated and will be removed in 2026.11.0. "
                "The required custom patterns have been added automatically for this release, but you must update your configuration.\n"
                "Please remove the 'provider' key and explicitly replace it with the following:\n\n"
                "custom_patterns:\n"
                '  - pattern: "L, TSTR"\n'
                '    name: "MeterID"\n'
                '    default_obis: "0.0.96.1.0.255"\n'
                '  - pattern: "F, TDTM"\n'
                '    name: "DateTime"\n'
                '    default_obis: "0.0.1.0.0.255"\n'
            )
            patterns = config.get(CONF_CUSTOM_PATTERNS, [])

            # Ensure "L, TSTR" for MeterID is present
            if not any(p.get(CONF_PATTERN) == "L, TSTR" for p in patterns):
                patterns.append(
                    {
                        CONF_PATTERN: "L, TSTR",
                        CONF_NAME: "MeterID",
                        CONF_DEFAULT_OBIS: [0, 0, 96, 1, 0, 255],
                        CONF_PRIORITY: 0,
                    }
                )

            # Ensure "F, TDTM" for DateTime is present
            if not any(p.get(CONF_PATTERN) == "F, TDTM" for p in patterns):
                patterns.append(
                    {
                        CONF_PATTERN: "F, TDTM",
                        CONF_NAME: "DateTime",
                        CONF_DEFAULT_OBIS: [0, 0, 1, 0, 0, 255],
                        CONF_PRIORITY: 0,
                    }
                )

            config[CONF_CUSTOM_PATTERNS] = patterns
        else:
            _LOGGER.warning(
                "The 'provider' option is deprecated and will be removed in 2026.11.0. "
                "The dlms_parser library now handles quirks dynamically. "
                "Please remove this option from your configuration."
            )
    return config


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
            cv.Optional(CONF_DECRYPTION_KEY): lambda value: cv.bind_key(
                value, name="Decryption key"
            ),
            cv.Optional(CONF_AUTH_KEY): lambda value: cv.bind_key(
                value, name="Authentication key"
            ),
            cv.Optional(CONF_CUSTOM_PATTERNS): cv.ensure_list(CUSTOM_PATTERN_SCHEMA),
            cv.Optional(CONF_SKIP_CRC, default=False): cv.boolean,
            cv.Optional(CONF_PROVIDER): cv.string,
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default="1000ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_provider_deprecation,
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("dlms_meter", require_rx=True)


async def to_code(config):
    dec_key_expr = cg.RawExpression("std::nullopt")
    if dec_key := config.get(CONF_DECRYPTION_KEY):
        key_bytes = [str(int(dec_key[i : i + 2], 16)) for i in range(0, 32, 2)]
        dec_key_expr = cg.RawExpression(
            f"std::array<uint8_t, 16>{{{', '.join(key_bytes)}}}"
        )

    auth_key_expr = cg.RawExpression("std::nullopt")
    if auth_key := config.get(CONF_AUTH_KEY):
        key_bytes = [str(int(auth_key[i : i + 2], 16)) for i in range(0, 32, 2)]
        auth_key_expr = cg.RawExpression(
            f"std::array<uint8_t, 16>{{{', '.join(key_bytes)}}}"
        )

    patterns = []
    if custom_patterns := config.get(CONF_CUSTOM_PATTERNS):
        for p in custom_patterns:
            name_expr = cg.RawExpression("std::nullopt")
            if name_val := p.get(CONF_NAME):
                name_expr = name_val

            if obis_vals := p.get(CONF_DEFAULT_OBIS):
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
        config[CONF_RECEIVE_TIMEOUT],
        config[CONF_SKIP_CRC],
        dec_key_expr,
        auth_key_expr,
        patterns_expr,
    )

    hub_id = config[CONF_ID].id

    sensor_count = 0
    for sens_conf in CORE.config.get("sensor", []):
        if (
            sens_conf.get("platform") == "dlms_meter"
            and sens_conf.get(CONF_DLMS_METER_ID).id == hub_id
        ):
            if CONF_OBIS_CODE in sens_conf:
                sensor_count += 1
            else:
                from .sensor import NUMERIC_KEYS

                sensor_count += sum(1 for key in NUMERIC_KEYS if key in sens_conf)

    text_sensor_count = 0
    for sens_conf in CORE.config.get("text_sensor", []):
        if (
            sens_conf.get("platform") == "dlms_meter"
            and sens_conf.get(CONF_DLMS_METER_ID).id == hub_id
        ):
            if CONF_OBIS_CODE in sens_conf:
                text_sensor_count += 1
            else:
                from .text_sensor import TEXT_KEYS

                text_sensor_count += sum(1 for key in TEXT_KEYS if key in sens_conf)

    binary_sensor_count = 0
    for sens_conf in CORE.config.get("binary_sensor", []):
        if (
            sens_conf.get("platform") == "dlms_meter"
            and sens_conf.get(CONF_DLMS_METER_ID).id == hub_id
        ):
            binary_sensor_count += 1

    cg.add_define("DLMS_MAX_SENSORS", sensor_count)
    cg.add_define("DLMS_MAX_TEXT_SENSORS", text_sensor_count)
    cg.add_define("DLMS_MAX_BINARY_SENSORS", binary_sensor_count)

    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CORE.is_esp32:
        esp32.add_idf_component(name="esphome/dlms_parser", ref="1.1.0")
    else:
        cg.add_library("esphome/dlms_parser", "1.1.0")
