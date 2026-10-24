from dataclasses import dataclass, field
import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.uart import DOMAIN as UART_DOMAIN
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_RECEIVE_TIMEOUT,
    CONF_RX_BUFFER_SIZE,
    CONF_UART_ID,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@glmnet", "@PolarGoose"]
DOMAIN = "dsmr"

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

MBUS_ID_KEYS = (CONF_GAS_MBUS_ID, CONF_WATER_MBUS_ID, CONF_THERMAL_MBUS_ID)


@dataclass
class DsmrData:
    # dicts used as ordered sets, so the generated field order is stable
    sensors: dict[str, None] = field(default_factory=dict)
    text_sensors: dict[str, None] = field(default_factory=dict)


def _get_data() -> DsmrData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = DsmrData()
        CORE.add_job(_emit_field_lists)
    return CORE.data[DOMAIN]


def register_sensor_field(key: str) -> None:
    _get_data().sensors[key] = None


def register_text_sensor_field(key: str) -> None:
    _get_data().text_sensors[key] = None


@coroutine_with_priority(CoroPriority.FINAL)
async def _emit_field_lists() -> None:
    # All instances share one parsed data type, so it holds the fields of every
    # sensor and text sensor configured on any instance.
    data = _get_data()
    if data.sensors:
        cg.add_define("DSMR_SENSOR_LIST_DEFINED")
        cg.add_define(
            "DSMR_SENSOR_LIST(F, sep)",
            cg.RawExpression(" sep ".join(f"F({key})" for key in data.sensors)),
        )
    if data.text_sensors:
        cg.add_define("DSMR_TEXT_SENSOR_LIST_DEFINED")
        cg.add_define(
            "DSMR_TEXT_SENSOR_LIST(F, sep)",
            cg.RawExpression(" sep ".join(f"F({key})" for key in data.text_sensors)),
        )


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Dsmr),
            cv.Optional(CONF_DECRYPTION_KEY): cv.bind_key(name="Decryption key"),
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


async def to_code(config: ConfigType) -> None:
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

    cg.add_library("esphome/dsmr_parser", "1.10.0")


def final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()

    # The M-Bus ids are compile-time constants of the parser library, shared by all instances
    for key in MBUS_ID_KEYS:
        if any(conf[key] != config[key] for conf in full_config[DOMAIN]):
            raise cv.Invalid(
                f"All '{DOMAIN}' instances must use the same '{key}'", path=[key]
            )

    for uart_conf in full_config[UART_DOMAIN]:
        if uart_conf[CONF_ID] == config[CONF_UART_ID]:
            rx_buffer_size = uart_conf[CONF_RX_BUFFER_SIZE]
            if rx_buffer_size < 1500:
                _LOGGER.warning(
                    "UART '%s' rx_buffer_size should be bigger than 1500 bytes to avoid packet losses (currently %d bytes).",
                    config[CONF_UART_ID],
                    rx_buffer_size,
                )
            break


FINAL_VALIDATE_SCHEMA = final_validate
