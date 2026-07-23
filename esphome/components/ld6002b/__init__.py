from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_WAKEUP_PIN

from .const import CONF_AUTO_WAKE, CONF_MAX_DATA_LEN, CONF_WAKEUP_PULSE_MS

CODEOWNERS = ["@hepter"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

ld6002b_ns = cg.esphome_ns.namespace("ld6002b")
LD6002BComponent = ld6002b_ns.class_("LD6002BComponent", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD6002BComponent),
            cv.Optional(CONF_WAKEUP_PIN): pins.gpio_output_pin_schema,
            cv.Optional(
                CONF_WAKEUP_PULSE_MS, default="50ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_AUTO_WAKE, default=True): cv.boolean,
            cv.Optional(CONF_MAX_DATA_LEN): cv.int_range(min=256, max=65535),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld6002b",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if wakeup_pin_config := config.get(CONF_WAKEUP_PIN):
        pin = await cg.gpio_pin_expression(wakeup_pin_config)
        cg.add(var.set_wakeup_pin(pin))

    if wakeup_pulse_config := config.get(CONF_WAKEUP_PULSE_MS):
        cg.add(var.set_wakeup_pulse_ms(wakeup_pulse_config.total_milliseconds))

    cg.add(var.set_auto_wake(config[CONF_AUTO_WAKE]))
    if max_data_len_config := config.get(CONF_MAX_DATA_LEN):
        cg.add(var.set_max_data_len(max_data_len_config))
