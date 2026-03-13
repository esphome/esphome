import re

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, PLATFORM_ESP32, PLATFORM_ESP8266

CODEOWNERS = ["@SimonFischer04"]
DEPENDENCIES = ["uart"]

CONF_DLMS_METER_ID = "dlms_meter_id"
CONF_DECRYPTION_KEY = "decryption_key"
CONF_PROVIDER = "provider"
CONF_TRANSPORT = "transport"

PROVIDERS = {"generic": 0, "netznoe": 1}
TRANSPORTS = {"mbus": 0, "raw": 1}

dlms_meter_component_ns = cg.esphome_ns.namespace("dlms_meter")
DlmsMeterComponent = dlms_meter_component_ns.class_(
    "DlmsMeterComponent", cg.Component, uart.UARTDevice
)


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
    # Validate standard OBIS format: A.B.C.D.E.F (e.g., 1.0.1.8.0.255)
    match = re.match(r"^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$", value)
    if match is None:
        raise cv.Invalid(
            f"{value} is not a valid OBIS code (expected format: A.B.C.D.E.F)"
        )

    # Normalize by converting each segment to an integer and back to a string
    # This strips leading zeros and allows enforcing the 0-255 range per field
    fields = value.split(".")
    normalized_fields = []

    for field in fields:
        num = int(field)
        if not (0 <= num <= 255):
            raise cv.Invalid(
                f"OBIS code field '{field}' in '{value}' is out of range (must be 0-255)."
            )
        normalized_fields.append(str(num))

    return ".".join(normalized_fields)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DlmsMeterComponent),
            cv.Optional(CONF_DECRYPTION_KEY): validate_key,
            cv.Optional(CONF_PROVIDER, default="generic"): cv.enum(
                PROVIDERS, lower=True
            ),
            cv.Optional(CONF_TRANSPORT, default="mbus"): cv.enum(
                TRANSPORTS, lower=True
            ),
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

    if CONF_DECRYPTION_KEY in config:
        key = ", ".join(str(b) for b in config[CONF_DECRYPTION_KEY])
        cg.add(var.set_decryption_key(cg.RawExpression(f"{{{key}}}")))

    cg.add(var.set_provider(PROVIDERS[config[CONF_PROVIDER]]))
    cg.add(var.set_transport(TRANSPORTS[config[CONF_TRANSPORT]]))
