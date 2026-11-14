from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_HARDWARE_UART,
    CONF_ID,
    CONF_LOGGER,
    CONF_RX_PIN,
    CONF_TX_PIN,
)
from esphome.components.const import CONF_RTS_PIN
from esphome.core import CORE
import esphome.final_validate as fv

AUTO_LOAD = ["sensor", "number"]
CODEOWNERS = ["@KoenBreeman"]
CONF_UART_NUM = "uart_num"
DOMAIN = "dmx_reader"

# Hardware UART names used by the logger component
_LOGGER_HW_UART_TO_NUM = {
    "UART0": 0,
    "UART1": 1,
    "UART2": 2,
}

dmx_reader_ns = cg.esphome_ns.namespace("dmx_reader")
DMXReader = dmx_reader_ns.class_("DMXReader", cg.Component)

register_channel_sensor = cg.slot_list(
    "DMX_READER_STATIC_SENSOR_COUNT",
    "register_channel_sensor",
    DOMAIN,
)
register_dynamic_channel_sensor = cg.slot_list(
    "DMX_READER_DYNAMIC_SENSOR_COUNT",
    "register_dynamic_channel_sensor",
    DOMAIN,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DMXReader),
        cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_RTS_PIN): pins.internal_gpio_output_pin_schema,
        # Default 2: on classic ESP32, logger owns UART0 and the first uart: bus gets UART1
        cv.Optional(CONF_UART_NUM, default=2): cv.int_range(min=0, max=2),
    }
).extend(cv.COMPONENT_SCHEMA)


def _logger_hardware_uart_num(full_config) -> int | None:
    """Return the hardware UART number the logger uses, or None if none."""
    logger_conf = full_config.get(CONF_LOGGER)
    if not logger_conf:
        return None
    if logger_conf.get(CONF_BAUD_RATE, 115200) == 0:
        return None  # serial logging disabled
    hw = logger_conf.get(CONF_HARDWARE_UART, "UART0")
    return _LOGGER_HW_UART_TO_NUM.get(hw)  # USB_* → None


def _uart_component_assigned_ports(full_config, logger_uart: int | None) -> set[int]:
    """
    Mirror IDFUARTComponent::setup() assignment order:
    start at 0, skip logger's hardware UART, assign one port per uart: entry.
    """
    used: set[int] = set()
    next_num = 0
    if logger_uart is not None and logger_uart == next_num:
        next_num += 1

    uart_conf = full_config.get("uart")
    if not uart_conf:
        return used

    # uart: can be a single dict or a list
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

    # Multiple dmx_reader instances must not share a port
    claimed = full_config.data.setdefault("dmx_reader_uart_ports", set())
    if uart_num in claimed:
        raise cv.Invalid(
            f"uart_num: {uart_num} is already used by another dmx_reader instance.",
            [CONF_UART_NUM],
        )
    claimed.add(uart_num)

    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    tx = await cg.gpio_pin_expression(config[CONF_TX_PIN])
    rx = await cg.gpio_pin_expression(config[CONF_RX_PIN])
    cg.add(var.set_tx_pin(tx))
    cg.add(var.set_rx_pin(rx))

    if CONF_RTS_PIN in config:
        rts = await cg.gpio_pin_expression(config[CONF_RTS_PIN])
        cg.add(var.set_rts_pin(rts))

    cg.add(var.set_uart_num(config[CONF_UART_NUM]))
    cg.add_library("someweisguy/esp_dmx", None)
