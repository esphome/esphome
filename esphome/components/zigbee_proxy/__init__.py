import esphome.codegen as cg
from esphome.components import uart, usb_uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_POWER_SAVE_MODE, CONF_WIFI
import esphome.final_validate as fv

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api", "uart"]

CONF_BUFFER_SIZE = "buffer_size"
CONF_INITIAL_TIMEOUT = "initial_timeout"
CONF_MIN_TIMEOUT = "min_timeout"
CONF_MAX_TIMEOUT = "max_timeout"
CONF_USB_UART_ID = "usb_uart_id"

# Default ACK timeout values calibrated for hardware UART (460800 baud, ~2-5 ms round-trip)
_DEFAULT_HW_INITIAL_TIMEOUT = 1600
_DEFAULT_HW_MIN_TIMEOUT = 400
_DEFAULT_HW_MAX_TIMEOUT = 3200

# Optimized ACK timeout values for USB CDC ACM paths (~3-5 ms round-trip with RX callback)
_DEFAULT_USB_INITIAL_TIMEOUT = 30
_DEFAULT_USB_MIN_TIMEOUT = 15
_DEFAULT_USB_MAX_TIMEOUT = 200

zigbee_proxy_ns = cg.esphome_ns.namespace("zigbee_proxy")
ZigbeeProxy = zigbee_proxy_ns.class_("ZigbeeProxy", cg.Component, uart.UARTDevice)


def final_validate(config):
    full_config = fv.full_config.get()
    if (wifi_conf := full_config.get(CONF_WIFI)) and (
        wifi_conf.get(CONF_POWER_SAVE_MODE, "").lower() != "none"
    ):
        raise cv.Invalid(
            f"{CONF_WIFI} {CONF_POWER_SAVE_MODE} must be set to 'none' when using Zigbee proxy"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZigbeeProxy),
            cv.Optional(CONF_BUFFER_SIZE): cv.SplitDefault(
                cv.int_range(min=256, max=2048),
                esp8266=512,
                default=1024,
            ),
            # When usb_uart_id is present the component registers an RX callback
            # for zero-wakeup-cycle data delivery and selects USB-optimized ACK
            # timeout defaults.  Explicit timeout keys always win.
            cv.Optional(CONF_USB_UART_ID): cv.use_id(usb_uart.USBUartChannel),
            cv.Optional(CONF_INITIAL_TIMEOUT): cv.int_range(min=10, max=10000),
            cv.Optional(CONF_MIN_TIMEOUT): cv.int_range(min=10, max=5000),
            cv.Optional(CONF_MAX_TIMEOUT): cv.int_range(min=50, max=10000),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
)

FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add_define("USE_ZIGBEE_PROXY")

    # Request UART to wake the main loop when data arrives for low-latency processing
    uart.request_wake_loop_on_rx()

    # Set buffer size via define for compile-time allocation
    if CONF_BUFFER_SIZE in config:
        cg.add_define("ZIGBEE_PROXY_BUFFER_SIZE", config[CONF_BUFFER_SIZE])

    # Select timeout defaults based on UART transport type.
    # USB CDC ACM with the RX callback has ~3-5 ms round-trip latency; hardware
    # UART is similar (~2-5 ms).  Different defaults are kept so that future
    # non-callback USB paths still get conservative starting values.
    is_usb = CONF_USB_UART_ID in config
    if is_usb:
        cg.add_define("USE_ZIGBEE_PROXY_USB_UART")
        usb_ch = await cg.get_variable(config[CONF_USB_UART_ID])
        cg.add(var.set_usb_uart_channel(usb_ch))

    initial_timeout = config.get(
        CONF_INITIAL_TIMEOUT,
        _DEFAULT_USB_INITIAL_TIMEOUT if is_usb else _DEFAULT_HW_INITIAL_TIMEOUT,
    )
    min_timeout = config.get(
        CONF_MIN_TIMEOUT,
        _DEFAULT_USB_MIN_TIMEOUT if is_usb else _DEFAULT_HW_MIN_TIMEOUT,
    )
    max_timeout = config.get(
        CONF_MAX_TIMEOUT,
        _DEFAULT_USB_MAX_TIMEOUT if is_usb else _DEFAULT_HW_MAX_TIMEOUT,
    )

    cg.add(var.set_initial_timeout(initial_timeout))
    cg.add(var.set_min_timeout(min_timeout))
    cg.add(var.set_max_timeout(max_timeout))
