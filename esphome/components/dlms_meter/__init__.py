import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RAW_DATA_ID, PLATFORM_ESP32, PLATFORM_ESP8266

CODEOWNERS = ["@SimonFischer04"]
DEPENDENCIES = ["uart"]

CONF_DLMS_METER_ID = "dlms_meter_id"
CONF_DECRYPTION_KEY = "decryption_key"
CONF_PROVIDER = "provider"

PROVIDERS = {"generic": 0, "netznoe": 1}

dlms_meter_component_ns = cg.esphome_ns.namespace("dlms_meter")
DlmsMeterComponent = dlms_meter_component_ns.class_(
    "DlmsMeterComponent", cg.Component, uart.UARTDevice
)


def validate_key(value):
    value = cv.string_strict(value)
    parts = [value[i : i + 2] for i in range(0, len(value), 2)]
    if len(parts) != 16:
        raise cv.Invalid("Decryption key must consist of 16 hexadecimal numbers")
    parts_int = []
    if any(len(part) != 2 for part in parts):
        raise cv.Invalid("Decryption key must be format XX")
    for part in parts:
        try:
            parts_int.append(int(part, 16))
        except ValueError:
            # pylint: disable=raise-missing-from
            raise cv.Invalid("Decryption key must be hex values from 00 to FF")

    return "".join(f"{part:02X}" for part in parts_int)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DlmsMeterComponent),
            cv.Required(CONF_DECRYPTION_KEY): validate_key,
            cv.Optional(CONF_PROVIDER, default="generic"): cv.enum(
                PROVIDERS, lower=True
            ),
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP8266, PLATFORM_ESP32]),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    key_hex = config[CONF_DECRYPTION_KEY]
    rhs_key_bytes = [int(key_hex[i : i + 2], 16) for i in range(0, len(key_hex), 2)]
    key_array = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs_key_bytes)
    cg.add(var.set_decryption_key(key_array, len(rhs_key_bytes)))

    if CONF_PROVIDER in config:
        cg.add(var.set_provider(PROVIDERS[config[CONF_PROVIDER]]))
