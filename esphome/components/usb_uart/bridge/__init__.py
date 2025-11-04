from esphome import pins
import esphome.codegen as cg
from esphome.components import uart, usb_cdc_acm
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UART_ID

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["tinyusb", "uart", "usb_cdc_acm"]

CONF_DTR_PIN = "dtr_pin"
CONF_RTS_PIN = "rts_pin"
CONF_USB_CDC_ACM_ID = "usb_cdc_acm_id"
CONF_UART_RX_BUFFER_SIZE = "uart_rx_buffer_size"
CONF_UART_TX_BUFFER_SIZE = "uart_tx_buffer_size"

usb_uart_bridge_ns = cg.esphome_ns.namespace("usb_uart_bridge")
USBUARTBridge = usb_uart_bridge_ns.class_("USBUARTBridge", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(USBUARTBridge),
        cv.Required(CONF_UART_ID): cv.use_id(uart.IDFUARTComponent),
        cv.Required(CONF_USB_CDC_ACM_ID): cv.use_id(usb_cdc_acm.USBCDCACMInstance),
        cv.Optional(CONF_DTR_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_RTS_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_UART_RX_BUFFER_SIZE, default=256): cv.uint16_t,
        cv.Optional(CONF_UART_TX_BUFFER_SIZE, default=256): cv.uint16_t,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    uart_component = await cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_uart_parent(uart_component))

    usb_cdc = await cg.get_variable(config[CONF_USB_CDC_ACM_ID])
    cg.add(var.set_usb_cdc_parent(usb_cdc))

    if dtr_pin_config := config.get(CONF_DTR_PIN):
        dtr_pin = await cg.gpio_pin_expression(dtr_pin_config)
        cg.add(var.set_dtr_pin(dtr_pin))
    if rts_pin_config := config.get(CONF_RTS_PIN):
        rts_pin = await cg.gpio_pin_expression(rts_pin_config)
        cg.add(var.set_rts_pin(rts_pin))

    cg.add_define(
        "USB_UART_BRIDGE_UART_RX_BUFFER_SIZE", config[CONF_UART_RX_BUFFER_SIZE]
    )
    cg.add_define(
        "USB_UART_BRIDGE_UART_TX_BUFFER_SIZE", config[CONF_UART_TX_BUFFER_SIZE]
    )
