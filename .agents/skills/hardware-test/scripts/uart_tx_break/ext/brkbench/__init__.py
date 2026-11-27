"""brkbench: LibreTiny TX break bench on a looped UART (esphome#22986).

After 8 s it runs CYCLES cycles: loopback check, break held about 500 ms with the probe pin read against a pull
opposite to the expected level and a driven-high positive control, release, loopback check; every 4th cycle holds
two breaks back to back. Ends with one SUMMARY line. fix: free the pins with pinModeRemove() and reopen the port
after each release (test-only workaround for the RTL8720C pin mux). inv_uart: a port with an inverted tx_pin, where
set_tx_break(true) must return false.
"""

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]

ns = cg.esphome_ns.namespace("brkbench")
BrkBench = ns.class_("BrkBench", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BrkBench),
            cv.Required("tx_gpio"): pins.gpio_output_pin_schema,
            cv.Required("probe_gpio"): pins.gpio_input_pin_schema,
            cv.Optional("probe_is_rx", default=False): cv.boolean,
            cv.Optional("loopback", default=True): cv.boolean,
            cv.Optional("cycles", default=16): cv.int_,
            cv.Optional("probe_from", default=1): cv.int_,
            cv.Optional("fix", default=False): cv.boolean,
            cv.Optional("rx_num", default=0): cv.int_,
            cv.Optional("inv_uart"): cv.use_id(uart.UARTComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    if hasattr(uart, "request_tx_break"):
        uart.request_tx_break()
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    tx = await cg.gpio_pin_expression(config["tx_gpio"])
    pr = await cg.gpio_pin_expression(config["probe_gpio"])
    cg.add(var.set_pins(tx, pr))
    cg.add(var.set_opts(config["probe_is_rx"], config["loopback"], config["cycles"]))
    cg.add(var.set_mode(config["probe_from"], config["fix"], config["rx_num"]))
    if "inv_uart" in config:
        inv = await cg.get_variable(config["inv_uart"])
        cg.add(var.set_inv(inv))
