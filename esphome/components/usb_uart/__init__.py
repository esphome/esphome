import esphome.codegen as cg
from esphome.components.esp32.const import KEY_ESP32, KEY_VARIANT
from esphome.components.uart import (
    CONF_DATA_BITS,
    CONF_PARITY,
    CONF_STOP_BITS,
    UARTComponent,
)
from esphome.components.usb_host import register_usb_client, usb_device_schema, USBClient
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_BUFFER_SIZE,
    CONF_CHANNELS,
    CONF_DEBUG,
    CONF_DUMMY_RECEIVER,
    CONF_ID,
)
from esphome.core import CORE
from esphome.cpp_types import Component

AUTO_LOAD = ["uart", "usb_host", "bytebuffer"]
CODEOWNERS = ["@clydebarrow"]

usb_uart_ns = cg.esphome_ns.namespace("usb_uart")
USBUartComponent = usb_uart_ns.class_("USBUartComponent", USBClient)
USBUartChannel = usb_uart_ns.class_("USBUartChannel", UARTComponent)

UARTParityOptions = usb_uart_ns.enum("UARTParityOptions")
UART_PARITY_OPTIONS = {
    "NONE": UARTParityOptions.UART_CONFIG_PARITY_NONE,
    "EVEN": UARTParityOptions.UART_CONFIG_PARITY_EVEN,
    "ODD": UARTParityOptions.UART_CONFIG_PARITY_ODD,
    "MARK": UARTParityOptions.UART_CONFIG_PARITY_MARK,
    "SPACE": UARTParityOptions.UART_CONFIG_PARITY_SPACE,
}

UARTStopBitsOptions = usb_uart_ns.enum("UARTStopBitsOptions")
UART_STOP_BITS_OPTIONS = {
    "1": UARTStopBitsOptions.UART_CONFIG_STOP_BITS_1,
    "1.5": UARTStopBitsOptions.UART_CONFIG_STOP_BITS_1_5,
    "2": UARTStopBitsOptions.UART_CONFIG_STOP_BITS_2,
}

DEFAULT_BAUD_RATE = 9600
# By default, log in hex format when no specific sequence is provided.
CONF_DEBUG_PREFIX = "debug_prefix"
CONF_DEBUG_ADD_SETTINGS = "debug_add_uart_settings"


class Type:
    def __init__(self, name, vid, pid, cls, max_channels=1, baud_rate_required=True):
        self.name = name
        cls = cls or name
        self.vid = vid
        self.pid = pid
        self.cls = usb_uart_ns.class_(f"USBUartType{cls}", USBUartComponent)
        self.max_channels = max_channels
        self.baud_rate_required = baud_rate_required


def get_target_variant():
    return CORE.data.get(KEY_ESP32, {}).get(KEY_VARIANT, "")


uart_types = (
    Type("FT23XX", 0x0403, 0x6010, "FT23XX", 4),
    Type("CH934X", 0x1A86, 0x55D9, "CH934X", 8),
    Type("CH34X", 0x1A86, 0x55D5, "CH34X", 4),
    Type("CH340", 0x1A86, 0x7523, "CH34X", 1),
    Type("ESP_JTAG", 0x303A, 0x1001, "CdcAcm", 1, baud_rate_required=False),
    Type("STM32_VCP", 0x0483, 0x5740, "CdcAcm", 1, baud_rate_required=False),
    # Type("CDC_ACM", 0, 0, "CdcAcm", 1, baud_rate_required=False),
    Type("CP210X", 0x10C4, 0xEA60, "CP210X", 4),
)


def channel_schema(max_channels, baud_rate_required, class_name):
    available_channels = 7 if "P4" in get_target_variant() else 3
    return cv.Schema(
        {
            cv.Required(CONF_CHANNELS): cv.All(
                cv.ensure_list(
                    cv.Schema(
                        {
                            cv.GenerateID(): cv.declare_id(USBUartChannel),
                            cv.Optional(CONF_BUFFER_SIZE, default=256): cv.int_range(
                                min=64, max=8192
                            ),
                            (
                                cv.Required(CONF_BAUD_RATE)
                                if baud_rate_required
                                else cv.Optional(
                                    CONF_BAUD_RATE, default=DEFAULT_BAUD_RATE
                                )
                            ): cv.int_range(min=300, max=1000000),
                            cv.Optional(CONF_STOP_BITS, default="1"): cv.enum(
                                UART_STOP_BITS_OPTIONS, upper=True
                            ),
                            cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
                                UART_PARITY_OPTIONS, upper=True
                            ),
                            cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(
                                min=5, max=8
                            ),
                            cv.Optional(CONF_DUMMY_RECEIVER, default=False): cv.boolean,
                            cv.Optional(CONF_DEBUG, default=False): cv.boolean,
                            cv.Optional(CONF_DEBUG_PREFIX, default=""): cv.string,
                            cv.Optional(
                                CONF_DEBUG_ADD_SETTINGS, default=False
                            ): cv.boolean,
                        }
                    )
                ),
                cv.Length(max=max_channels)
                if class_name == "CH934X" or available_channels >= max_channels
                else cv.Length(max=available_channels),
            )
        }
    )


CONFIG_SCHEMA = cv.ensure_list(
    cv.typed_schema(
        {
            it.name: usb_device_schema(it.cls, it.vid, it.pid).extend(
                channel_schema(it.max_channels, it.baud_rate_required, it.cls)
            )
            for it in uart_types
        },
        upper=True,
    )
)


async def to_code(config):
    # Get the USBHost instance from CORE.data
    # It should have been stored by the usb_host component's to_code()
    usb_host_var = CORE.data.get("usb_host_instance")
    if usb_host_var is None:
        raise cv.Invalid("usb_uart requires a usb_host component to be configured")

    for device in config:
        var = await register_usb_client(device, usb_host_var)
        for index, channel in enumerate(device[CONF_CHANNELS]):
            chvar = cg.new_Pvariable(channel[CONF_ID], index, channel[CONF_BUFFER_SIZE])
            await cg.register_parented(chvar, var)
            cg.add(chvar.set_rx_buffer_size(channel[CONF_BUFFER_SIZE]))
            cg.add(chvar.set_stop_bits(channel[CONF_STOP_BITS]))
            cg.add(chvar.set_data_bits(channel[CONF_DATA_BITS]))
            cg.add(chvar.set_parity(channel[CONF_PARITY]))
            cg.add(chvar.set_baud_rate(channel[CONF_BAUD_RATE]))
            cg.add(chvar.set_dummy_receiver(channel[CONF_DUMMY_RECEIVER]))
            cg.add(chvar.set_debug(channel[CONF_DEBUG]))
            if CONF_DEBUG_PREFIX in channel:
                cg.add(chvar.set_debug_prefix(channel[CONF_DEBUG_PREFIX]))
            if channel[CONF_DEBUG_ADD_SETTINGS]:
                cg.add(chvar.set_debug_add_settings(channel[CONF_DEBUG_ADD_SETTINGS]))
                cg.add_define("UART_DEBUGGER_ADD_SETTINGS")
            cg.add(var.add_channel(chvar))
            if channel[CONF_DEBUG]:
                cg.add_define("USE_UART_DEBUGGER")
