import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
)

CODEOWNERS = ["@glmnet", "@zuidwijk"]

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]

CONF_DSMR_ID = "dsmr_id"
CONF_DECRYPTION_KEY = "decryption_key"

# Hack to prevent compile error due to ambiguity with lib namespace
dsmr_ns = cg.esphome_ns.namespace("esphome::dsmr")
Dsmr = dsmr_ns.class_("Dsmr", cg.Component, uart.UARTDevice)


def _validate_key(value):
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


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Dsmr),
        cv.Optional(CONF_DECRYPTION_KEY): _validate_key,
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    if CONF_DECRYPTION_KEY in config:
        cg.add(var.set_decryption_key(config[CONF_DECRYPTION_KEY]))
    await cg.register_component(var, config)

    # DSMR Parser
    cg.add_library("glmnet/Dsmr", "0.3")

    # Crypto
    cg.add_library("rweather/Crypto", "0.2.0")
