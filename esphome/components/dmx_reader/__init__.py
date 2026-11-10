import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_RX_PIN, CONF_TX_PIN

CODEOWNERS = ["@KoenBreeman"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["sensor", "number"]

CONF_RTS_PIN = "rts_pin"
CONF_UART_NUM = "uart_num"

dmx_reader_ns = cg.esphome_ns.namespace("dmx_reader")
DMXReader = dmx_reader_ns.class_("DMXReader", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DMXReader),
        cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_RTS_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_UART_NUM, default=1): cv.int_range(min=0, max=2),
    }
).extend(cv.COMPONENT_SCHEMA)


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
    cg.add_library("someweisguy/esp_dmx",None)
