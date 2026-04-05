import re

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, PLATFORM_ESP32, PLATFORM_ESP8266
from esphome.core import CORE

CODEOWNERS = ["@SimonFischer04", "@Tomer27cz", "@latonita", "@PolarGoose"]
DEPENDENCIES = ["uart"]

CONF_DLMS_METER_ID = "dlms_meter_id"
CONF_DECRYPTION_KEY = "decryption_key"
CONF_AUTH_KEY = "auth_key"
CONF_OBIS_CODE = "obis_code"
CONF_CUSTOM_PATTERNS = "custom_patterns"
CONF_SKIP_CRC = "skip_crc"

dlms_meter_component_ns = cg.esphome_ns.namespace("dlms_meter")
DlmsMeterComponent = dlms_meter_component_ns.class_(
    "DlmsMeterComponent", cg.Component, uart.UARTDevice
)

# Maintain backwards compatibility mappings
NUMERIC_KEYS = {
    "voltage_l1": "1-0:32.7.0",
    "voltage_l2": "1-0:52.7.0",
    "voltage_l3": "1-0:72.7.0",
    "current_l1": "1-0:31.7.0",
    "current_l2": "1-0:51.7.0",
    "current_l3": "1-0:71.7.0",
    "active_power_plus": "1-0:1.7.0",
    "active_power_minus": "1-0:2.7.0",
    "active_energy_plus": "1-0:1.8.0",
    "active_energy_minus": "1-0:2.8.0",
    "reactive_energy_plus": "1-0:3.8.0",
    "reactive_energy_minus": "1-0:4.8.0",
    "power_factor": "1-0:13.7.0",
}

TEXT_KEYS = {
    "timestamp": "0-0:1.0.0",
    "meternumber": "0-0:96.1.0",
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
    value = cv.string(value)
    # Validate flexible OBIS format, e.g., 1.0.1.8.0.255 or 1-0:32.7.0
    match = re.match(r"^[0-9\.\-\:\*]+$", value)
    if match is None:
        raise cv.Invalid(f"{value} is not a valid OBIS code format")
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DlmsMeterComponent),
            cv.Optional(CONF_DECRYPTION_KEY): validate_key,
            cv.Optional(CONF_AUTH_KEY): validate_key,
            cv.Optional(CONF_CUSTOM_PATTERNS): cv.ensure_list(cv.string),
            cv.Optional(CONF_SKIP_CRC, default=False): cv.boolean,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP8266, PLATFORM_ESP32]),
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("dlms_meter", require_rx=True)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CORE.is_esp8266:
        # Force PlatformIO to add the nested bearssl folder to the include path
        cg.add_build_flag(
            "-I${PROJECT_PACKAGES_DIR}/framework-arduinoespressif8266/tools/sdk/include/bearssl"
        )

    cg.add(var.set_skip_crc_check(config[CONF_SKIP_CRC]))

    if CONF_DECRYPTION_KEY in config:
        key = ", ".join(str(b) for b in config[CONF_DECRYPTION_KEY])
        cg.add(var.set_decryption_key(cg.RawExpression(f"{{{key}}}")))

    if CONF_AUTH_KEY in config:
        auth = ", ".join(str(b) for b in config[CONF_AUTH_KEY])
        cg.add(var.set_authentication_key(cg.RawExpression(f"{{{auth}}}")))

    if CONF_CUSTOM_PATTERNS in config:
        for pattern in config[CONF_CUSTOM_PATTERNS]:
            cg.add(var.add_custom_pattern(pattern))

    cg.add_library("dlms_parser", None, "https://github.com/esphome-libs/dlms_parser")
