import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components.const import CONF_RTS_PIN
from esphome.components.esp32 import (
    add_idf_component,
    get_esp32_variant,
    variant_filtered_enum,
)
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32H4,
    VARIANT_ESP32H21,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    VARIANTS,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_HARDWARE_UART,
    CONF_ID,
    CONF_LOGGER,
    CONF_MODE,
    CONF_RX_PIN,
    CONF_TX_PIN,
)
import esphome.final_validate as fv

AUTO_LOAD = ["light", "sensor", "number"]
CODEOWNERS = ["@KoenBreeman"]
CONF_UART_NUM = "uart_num"
CONF_DMX_ID = "dmx_id"
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
DMXMode = dmx_ns.enum("DMXMode", is_class=True)

UART_CONFIG = {
    VARIANT_ESP32: (2, (0, 1, 2)),
    VARIANT_ESP32C2: (1, (0, 1)),
    VARIANT_ESP32C3: (1, (0, 1)),
    VARIANT_ESP32C5: (1, (0, 1)),
    VARIANT_ESP32C6: (1, (0, 1)),
    VARIANT_ESP32C61: (1, (0, 1)),
    VARIANT_ESP32H2: (1, (0, 1)),
    VARIANT_ESP32H4: (1, (0, 1)),
    VARIANT_ESP32H21: (1, (0, 1)),
    VARIANT_ESP32P4: (1, (0, 1)),
    VARIANT_ESP32S2: (1, (0, 1)),
    VARIANT_ESP32S3: (2, (0, 1, 2)),
    VARIANT_ESP32S31: (2, (0, 1, 2, 3)),
}

assert set(VARIANTS) <= set(UART_CONFIG), (
    "Missing UART configuration for ESP32 variants: "
    f"{set(VARIANTS) - set(UART_CONFIG)}"
)

UART_NUMBERS = variant_filtered_enum(
    {
        variant: allowed
        for variant, (_, allowed) in UART_CONFIG.items()
    }
)

def default_uart_num():
    return UART_CONFIG[get_esp32_variant()][0]


MODE_TO_ENUM = {
    MODE_RX: DMXMode.DMX_MODE_RX,
    MODE_TX: DMXMode.DMX_MODE_TX,
    MODE_BIDI: DMXMode.DMX_MODE_BIDIRECTIONAL,
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
        cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_number,
        cv.Optional(CONF_RTS_PIN): pins.internal_gpio_pin_number,
        cv.Optional(
            CONF_UART_NUM,
            default=default_uart_num,
        ): UART_NUMBERS,
        cv.Optional(CONF_MODE, default=MODE_RX): cv.one_of(
            MODE_RX, MODE_TX, MODE_BIDI, lower=True
        ),
        cv.Optional(
            CONF_TX_INTERVAL, default="25ms"
        ): cv.positive_time_period_milliseconds,
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


def _final_validate(config) -> None:
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
