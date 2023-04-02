import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_RECEIVE_TIMEOUT,
)

CODEOWNERS = ["@middelink"]

MULTI_CONF = True

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]

CONF_KAMSTRUP_ID = "kamstrup_id"
CONF_BUNDLE_REQUESTS = "bundle_requests"

kamstrup_ns = cg.esphome_ns.namespace("kamstrup")
Kamstrup = kamstrup_ns.class_("Kamstrup", cg.PollingComponent, uart.UARTDevice)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Kamstrup),
            cv.Optional(CONF_BUNDLE_REQUESTS): cv.uint8_t,
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default="2s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("1h"))
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "kamstrup",
    baud_rate=1200,
    require_tx=True,
    require_rx=True,
    parity=None,
    stop_bits=2,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_receive_timeout(config[CONF_RECEIVE_TIMEOUT].total_milliseconds))
    if CONF_BUNDLE_REQUESTS in config:
        cg.add(var.set_bundle_requests(config[CONF_BUNDLE_REQUESTS]))
