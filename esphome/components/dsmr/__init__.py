import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]

CONF_DSMR_ID = "dsmr_id"
CONF_DECRYPTION_KEY = "decryption_key"

dsmr_ns = cg.esphome_ns.namespace("dsmr_")
DSMR = dsmr_ns.class_("Dsmr", cg.Component, uart.UARTDevice)


def validate_key(value):
    if isinstance(value, list) and len(value) == 16:
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("key must be 16 bytes")


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DSMR),
        cv.Optional(CONF_DECRYPTION_KEY): validate_key,
    }
).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    uart_component = yield cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    if CONF_DECRYPTION_KEY in config:
        cg.add(var.set_decryption_key(config[CONF_DECRYPTION_KEY]))
    yield cg.register_component(var, config)

    # Crypto
    cg.add_library("1168", "0.2.0")