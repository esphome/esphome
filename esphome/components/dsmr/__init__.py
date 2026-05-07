import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_RECEIVE_TIMEOUT,
    CONF_RX_BUFFER_SIZE,
    CONF_UART_ID,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@glmnet", "@PolarGoose"]

MULTI_CONF = True

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]

CONF_CRC_CHECK = "crc_check"
CONF_DECRYPTION_KEY = "decryption_key"
CONF_DSMR_ID = "dsmr_id"
CONF_GAS_MBUS_ID = "gas_mbus_id"
CONF_WATER_MBUS_ID = "water_mbus_id"
CONF_THERMAL_MBUS_ID = "thermal_mbus_id"
CONF_MAX_TELEGRAM_LENGTH = "max_telegram_length"
CONF_REQUEST_INTERVAL = "request_interval"
CONF_REQUEST_PIN = "request_pin"

dsmr_ns = cg.esphome_ns.namespace("dsmr")
Dsmr = dsmr_ns.class_("Dsmr", cg.Component, uart.UARTDevice)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Dsmr),
            cv.Optional(CONF_DECRYPTION_KEY): lambda value: cv.bind_key(
                value, name="Decryption key"
            ),
            cv.Optional(CONF_CRC_CHECK, default=True): cv.boolean,
            cv.Optional(CONF_GAS_MBUS_ID, default=1): cv.int_,
            cv.Optional(CONF_WATER_MBUS_ID, default=2): cv.int_,
            cv.Optional(CONF_THERMAL_MBUS_ID, default=3): cv.int_,
            cv.Optional(CONF_MAX_TELEGRAM_LENGTH, default=1500): cv.int_range(min=1),
            cv.Optional(CONF_REQUEST_PIN): pins.gpio_output_pin_schema,
            cv.Optional(
                CONF_REQUEST_INTERVAL, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default="200ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    if CONF_REQUEST_PIN in config:
        request_pin = await cg.gpio_pin_expression(config[CONF_REQUEST_PIN])
    else:
        request_pin = cg.nullptr
    decryption_key = config.get(CONF_DECRYPTION_KEY)
    if decryption_key is None:
        decryption_key = cg.nullptr
    var = cg.new_Pvariable(
        config[CONF_ID],
        uart_component,
        config[CONF_CRC_CHECK],
        config[CONF_MAX_TELEGRAM_LENGTH],
        config[CONF_REQUEST_INTERVAL].total_milliseconds,
        config[CONF_RECEIVE_TIMEOUT].total_milliseconds,
        request_pin,
        decryption_key,
    )
    await cg.register_component(var, config)

    cg.add_build_flag("-DDSMR_GAS_MBUS_ID=" + str(config[CONF_GAS_MBUS_ID]))
    cg.add_build_flag("-DDSMR_WATER_MBUS_ID=" + str(config[CONF_WATER_MBUS_ID]))
    cg.add_build_flag("-DDSMR_THERMAL_MBUS_ID=" + str(config[CONF_THERMAL_MBUS_ID]))

    cg.add_library("esphome/dsmr_parser", "1.4.0")


def final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()

    for uart_conf in full_config["uart"]:
        if uart_conf[CONF_ID] == config[CONF_UART_ID]:
            rx_buffer_size = uart_conf[CONF_RX_BUFFER_SIZE]
            if rx_buffer_size < 1500:
                _LOGGER.warning(
                    "UART '%s' rx_buffer_size should be bigger than 1500 bytes to avoid packet losses (currently %d bytes).",
                    config[CONF_UART_ID],
                    rx_buffer_size,
                )
            break

    return config


FINAL_VALIDATE_SCHEMA = final_validate
