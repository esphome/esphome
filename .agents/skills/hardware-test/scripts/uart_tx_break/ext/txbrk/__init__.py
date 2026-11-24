"""txbrk: hold and release a TX break once per run_next() call and log one CYC line per cycle (esphome#22986).

Needs a tree with uart.request_tx_break() and UARTComponent::set_tx_break().
"""

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]
MULTI_CONF = True
ns = cg.esphome_ns.namespace("txbrk")
TxBrk = ns.class_("TxBrk", cg.Component)
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TxBrk),
        cv.Required("uart_id"): cv.use_id(uart.UARTComponent),
        cv.Optional("tx_gpio", default=-1): cv.int_,
        cv.Optional("rx_gpio", default=-1): cv.int_,
        cv.Optional("label", default="bus"): cv.string,
        cv.Optional("loopback", default=False): cv.boolean,
        cv.Optional("live", default=False): cv.boolean,
        # ESP8266 hardware UART1 (TX on GPIO2): read its TX level and registers through its own receiver
        cv.Optional("hw_uart1", default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    uart.request_tx_break()
    u = await cg.get_variable(config["uart_id"])
    var = cg.new_Pvariable(
        config[CONF_ID],
        u,
        config["tx_gpio"],
        config["rx_gpio"],
        config["label"],
        config["loopback"],
        config["live"],
        config["hw_uart1"],
    )
    await cg.register_component(var, config)
