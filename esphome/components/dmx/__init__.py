from esphome import pins
import esphome.codegen as cg
from esphome.components.const import CONF_RTS_PIN
from esphome.components.esp32 import add_idf_component
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_HARDWARE_UART,
    CONF_ID,
    CONF_LOGGER,
    CONF_RX_PIN,
    CONF_TX_PIN,
)
import esphome.final_validate as fv
import logging

CODEOWNERS = ["@KoenBreeman"]
CONF_UART_NUM = "uart_num"
CONF_DMX_ID = "dmx_id"
CONF_MODE = "mode"
CONF_TX_INTERVAL = "tx_interval"
CONF_RX_GUARD = "rx_guard"
DEPENDENCIES = ["esp32"]
DOMAIN = "dmx"
_LOGGER = logging.getLogger(__name__)

MODE_RX = "rx"
MODE_TX = "tx"
MODE_BIDI = "bidirectional"

_LOGGER_HW_UART_TO_NUM = {
    "UART0": 0,
    "UART1": 1,
    "UART2": 2,
}

dmx_ns = cg.esphome_ns.namespace("dmx")
DMXComponent = dmx_ns.class_("DMXComponent", cg.Component)
DMXMode = dmx_ns.enum("DMXMode")

MODE_TO_ENUM = {
    MODE_RX: DMXMode.RX,
    MODE_TX: DMXMode.TX,
    MODE_BIDI: DMXMode.BIDIRECTIONAL,
}

register_channel_sensor = cg.slot_list(
    "DMX_STATIC_SENSOR_COUNT",
    "register_channel_sensor",
    DOMAIN,
)
register_dynamic_channel_sensor = cg.slot_list(
    "DMX_DYNAMIC_SENSOR_COUNT",
    "register_dynamic_channel_sensor",
    DOMAIN,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DMXComponent),
        cv.Required(CONF_TX_PIN): pins.internal_gpio_pin_number,
        cv.Required(CONF_RX_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_RTS_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_UART_NUM, default=2): cv.int_range(min=0, max=2),
        cv.Optional(CONF_MODE, default=MODE_RX): cv.one_of(
            MODE_RX, MODE_TX, MODE_BIDI, lower=True
        ),
        cv.Optional(CONF_TX_INTERVAL, default="25ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RX_GUARD, default="5ms"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


def _logger_hardware_uart_num(full_config) -> int | None:
    logger_conf = full_config.get(CONF_LOGGER)
    if not logger_conf:
        return None
    if logger_conf.get(CONF_BAUD_RATE, 115200) == 0:
        return None
    hw = logger_conf.get(CONF_HARDWARE_UART, "UART0")
    return _LOGGER_HW_UART_TO_NUM.get(hw)


def _uart_component_assigned_ports(full_config, logger_uart: int | None) -> set[int]:
    used: set[int] = set()
    next_num = 0
    if logger_uart is not None and logger_uart == next_num:
        next_num += 1

    uart_conf = full_config.get("uart")
    if not uart_conf:
        return used

    entries = uart_conf if isinstance(uart_conf, list) else [uart_conf]
    for _ in entries:
        if logger_uart is not None and next_num == logger_uart:
            next_num += 1
        used.add(next_num)
        next_num += 1
    return used


def _final_validate(config):
    full_config = fv.full_config.get()
    uart_num = config[CONF_UART_NUM]

    mode = config[CONF_MODE]
    if mode in (MODE_TX, MODE_BIDI) and CONF_RTS_PIN not in config:
        _LOGGER.warning(
            "dmx: mode '%s' without rts_pin — direction is controlled by the "
            "transceiver (OK for M5 Unit DMX; boards with DE/RE should set rts_pin)",
            mode,
        )

    logger_uart = _logger_hardware_uart_num(full_config)
    if logger_uart is not None and uart_num == logger_uart:
        raise cv.Invalid(
            f"uart_num: {uart_num} conflicts with the logger hardware UART. "
            f"Choose a different uart_num, or set logger baud_rate: 0 / "
            f"hardware_uart to a USB option.",
            [CONF_UART_NUM],
        )

    uart_ports = _uart_component_assigned_ports(full_config, logger_uart)
    if uart_num in uart_ports:
        raise cv.Invalid(
            f"uart_num: {uart_num} conflicts with an ESPHome uart: bus "
            f"(ports assigned to uart: components: {sorted(uart_ports)}). "
            f"Pick a free uart_num, or remove/reconfigure the conflicting uart: entry.",
            [CONF_UART_NUM],
        )

    claimed = full_config.data.setdefault("dmx_uart_ports", set())
    if uart_num in claimed:
        raise cv.Invalid(
            f"uart_num: {uart_num} is already used by another dmx instance.",
            [CONF_UART_NUM],
        )
    claimed.add(uart_num)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_tx_pin(config[CONF_TX_PIN]))
    cg.add(var.set_rx_pin(config[CONF_RX_PIN]))
    if CONF_RTS_PIN in config:
        cg.add(var.set_rts_pin(config[CONF_RTS_PIN]))
    else:
        cg.add(var.set_rts_pin(-1))
    cg.add(var.set_uart_num(config[CONF_UART_NUM]))
    cg.add(var.set_mode(MODE_TO_ENUM[config[CONF_MODE]]))
    cg.add(var.set_tx_interval(config[CONF_TX_INTERVAL].total_milliseconds))
    cg.add(var.set_rx_guard(config[CONF_RX_GUARD].total_milliseconds))

    add_idf_component(
        name="esp_dmx",
        repo="https://github.com/KoenBreeman/esp_dmx.git",
        ref="fix/cmake-esp-driver-gptimer",
    )
