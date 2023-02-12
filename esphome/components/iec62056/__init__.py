import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_RECEIVE_TIMEOUT,
    CONF_UPDATE_INTERVAL,
)

CODEOWNERS = ["@aquaticus"]

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor", "switch", "binary_sensor"]
CONF_IEC62056_ID = "iec62056_id"
CONF_OBIS = "obis"
CONF_BATTERY_METER = "battery_meter"
CONF_RETRY_COUNTER_MAX = "retry_counter_max"
CONF_RETRY_DELAY = "retry_delay"
CONF_MODE_D = "mode_d"  # protocol mode D
CONF_BAUD_RATE_MAX = "baud_rate_max"

iec62056_ns = cg.esphome_ns.namespace("iec62056")
IEC62056Component = iec62056_ns.class_(
    "IEC62056Component", cg.Component, uart.UARTDevice
)


def validate_obis(value):
    # match, e.g.
    # F.35.90*00
    # F.35
    # 0.2.2
    # 0-0:1.0.0*102
    rx = r"(\d+-\d+\:){,1}[\dA-F]+\.\d+(.\d+){,1}(\*\d+){,1}"

    m = re.fullmatch(rx, value)
    if m is None:
        raise cv.Invalid(f"Invalid OBIS format: '{value}'")

    return value


def validate_baud_rate(value):
    if value > 0:
        baud_rates = [300, 600, 1200, 2400, 4800, 9600, 19200]
        if value not in baud_rates:
            raise cv.Invalid(f"Non standard baud rate {value}. Use one of {baud_rates}")

    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(IEC62056Component),
            cv.Optional(CONF_UPDATE_INTERVAL, default="15min"): cv.update_interval,
            cv.Optional(CONF_BAUD_RATE_MAX, default=9600): validate_baud_rate,
            cv.Optional(CONF_BATTERY_METER, default=False): cv.boolean,
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default="3s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_RETRY_COUNTER_MAX, default=2): cv.int_range(min=0, max=9),
            cv.Optional(
                CONF_RETRY_DELAY, default="15s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MODE_D, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_UPDATE_INTERVAL in config:
        cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))

    if CONF_BAUD_RATE_MAX in config:
        cg.add(var.set_config_baud_rate_max(config[CONF_BAUD_RATE_MAX]))

    if CONF_RECEIVE_TIMEOUT in config:
        cg.add(var.set_connection_timeout_ms(config[CONF_RECEIVE_TIMEOUT]))

    if CONF_BATTERY_METER in config:
        cg.add(var.set_battery_meter(config[CONF_BATTERY_METER]))

    if CONF_RETRY_COUNTER_MAX in config:
        cg.add(var.set_max_retry_counter(config[CONF_RETRY_COUNTER_MAX]))

    if CONF_RETRY_DELAY in config:
        cg.add(var.set_retry_delay(config[CONF_RETRY_DELAY]))

    if CONF_MODE_D in config:
        cg.add(var.set_mode_d(config[CONF_MODE_D]))
