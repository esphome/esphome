import esphome.codegen as cg
from esphome.components.const import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS
from esphome.components.esp32 import VARIANT_ESP32P4, get_esp32_variant
from esphome.components.uart import CONF_DEBUG_PREFIX, CONF_FLUSH_TIMEOUT, UARTComponent
from esphome.components.usb_host import (
    get_max_packet_size,
    get_max_transfer_requests,
    register_usb_client,
    usb_device_schema,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_BUFFER_SIZE,
    CONF_CHANNELS,
    CONF_DEBUG,
    CONF_DUMMY_RECEIVER,
    CONF_ID,
    CONF_TYPE,
)
from esphome.core import CORE
from esphome.cpp_types import Component
from esphome.types import ConfigType

AUTO_LOAD = ["uart", "usb_host", "bytebuffer"]
CODEOWNERS = ["@clydebarrow"]

usb_uart_ns = cg.esphome_ns.namespace("usb_uart")
USBUartComponent = usb_uart_ns.class_("USBUartComponent", Component)
USBUartChannel = usb_uart_ns.class_("USBUartChannel", UARTComponent)
CH934XChannel = usb_uart_ns.class_("CH934XChannel", UARTComponent)

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


class Type:
    def __init__(
        self,
        name: str,
        vid: int,
        pid: int,
        cls: str | None,
        max_channels: int = 1,
        baud_rate_required: bool = True,
        channel_cls=None,
        max_baud: int = 1_000_000,
    ) -> None:
        self.name = name
        cls = cls or name
        self.vid = vid
        self.pid = pid
        self.cls = usb_uart_ns.class_(f"USBUartType{cls}", USBUartComponent)
        self._max_channels = max_channels
        self.baud_rate_required = baud_rate_required
        self.channel_cls = channel_cls or USBUartChannel
        self.max_baud = max_baud

    # CDC-style devices use one bulk IN + one bulk OUT endpoint per channel.
    ENDPOINTS_PER_CHANNEL = 2

    @property
    def max_channels(self) -> int:
        # The USB host controller's endpoint budget caps the usable channel count:
        # 7 endpoints on ESP32-S3, 15 on ESP32-P4, one of which is always needed
        # for enumeration/control (EP0). CDC-style: (7-1)//2 = 3 on S3, (15-1)//2 = 7 on P4.
        total_endpoints = (
            15 if CORE.is_esp32 and get_esp32_variant() == VARIANT_ESP32P4 else 7
        )
        return min(
            self._max_channels, (total_endpoints - 1) // self.ENDPOINTS_PER_CHANNEL
        )


class MpxType(Type):
    @property
    def max_channels(self) -> int:
        # Multiplexed devices (CH934x) route all channels over one shared bulk
        # IN/OUT pair plus one command IN/OUT pair — 4 endpoints + EP0 in total,
        # independent of the channel count. The chip's own port count is the
        # only limit, so even all 8 CH348 channels fit on an ESP32-S3.
        return self._max_channels


uart_types = (
    MpxType(
        "CH9344",
        0x1A86,
        0xE018,
        "CH934X",
        4,
        channel_cls=CH934XChannel,
        max_baud=12_000_000,
    ),
    MpxType(
        "CH348",
        0x1A86,
        0x55D9,
        "CH934X",
        8,
        channel_cls=CH934XChannel,
        max_baud=12_000_000,
    ),
    Type("CDC_ACM", 0, 0, "CdcAcm", 1, baud_rate_required=False),
    Type("CH34X", 0x1A86, 0x55D5, "CH34X", 4, max_baud=2_000_000),
    Type("CH340", 0x1A86, 0x7523, "CH34X", 1, max_baud=2_000_000),
    Type("CP210X", 0x10C4, 0xEA60, "CP210X", 3, max_baud=2_000_000),
    Type("ESP_JTAG", 0x303A, 0x1001, "CdcAcm", 1, baud_rate_required=False),
    Type("FT232", 0x0403, 0x6001, "FT23XX", 1, max_baud=3_000_000),
    Type("FT2232", 0x0403, 0x6010, "FT23XX", 2, max_baud=12_000_000),
    Type("FT4232", 0x0403, 0x6011, "FT23XX", 4, max_baud=12_000_000),
    Type("PL2303", 0x067B, 0x2303, "PL2303", 1, max_baud=6_000_000),
    Type("PL2303GB", 0x067B, 0x23B3, "PL2303", 1, max_baud=6_000_000),
    Type("PL2303GC", 0x067B, 0x23A3, "PL2303", 1, max_baud=6_000_000),
    Type("PL2303GE", 0x067B, 0x23E3, "PL2303", 1, max_baud=6_000_000),
    Type("PL2303GL", 0x067B, 0x23D3, "PL2303", 1, max_baud=6_000_000),
    Type("PL2303GS", 0x067B, 0x23F3, "PL2303", 1, max_baud=6_000_000),
    Type("PL2303GT", 0x067B, 0x23C3, "PL2303", 1, max_baud=6_000_000),
    Type("STM32_VCP", 0x0483, 0x5740, "CdcAcm", 1, baud_rate_required=False),
)


def channel_schema(type_: "Type") -> cv.Schema:
    return cv.Schema(
        {
            cv.Required(CONF_CHANNELS): cv.All(
                cv.ensure_list(
                    cv.Schema(
                        {
                            cv.GenerateID(): cv.declare_id(type_.channel_cls),
                            cv.Optional(CONF_BUFFER_SIZE, default=256): cv.int_range(
                                min=64, max=8192
                            ),
                            (
                                cv.Required(CONF_BAUD_RATE)
                                if type_.baud_rate_required
                                else cv.Optional(
                                    CONF_BAUD_RATE, default=DEFAULT_BAUD_RATE
                                )
                            ): cv.int_range(min=300, max=type_.max_baud),
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
                                CONF_FLUSH_TIMEOUT, default="100ms"
                            ): cv.positive_time_period_milliseconds,
                        }
                    )
                ),
                cv.Length(
                    min=1,
                    max=type_.max_channels,
                    msg=f"Device type {type_.name} supports a maximum of {type_.max_channels} channels",
                ),
            )
        }
    )


CONFIG_SCHEMA = cv.ensure_list(
    cv.typed_schema(
        {
            it.name: usb_device_schema(it.cls, it.vid, it.pid).extend(
                channel_schema(it)
            )
            for it in uart_types
        },
        upper=True,
    )
)


async def to_code(config: list[ConfigType]) -> None:
    # The output chunk pool/queue are compile-time-sized templates shared by all
    # USBUartChannel instances, so use the largest buffer_size across every channel
    # of every device. Add one extra slot because LockFreeQueue<T,N> is a ring
    # buffer that wastes one entry.
    max_buffer_size = max(
        channel[CONF_BUFFER_SIZE]
        for device in config
        for channel in device[CONF_CHANNELS]
    )
    if isinstance(device[CONF_TYPE], MpxType):
        output_chunk_count = (
            ceil(max_buffer_size / (get_max_packet_size() - 3))
            * len(device[CONF_CHANNELS])
            + 1
        )
    else:
        output_chunk_count = max(max_buffer_size // get_max_packet_size(), 2) + 1
    cg.add_define("USB_UART_OUTPUT_CHUNK_COUNT", output_chunk_count)

    # Multiplexed (CH934x) drivers initialise their channels in parallel over a single
    # shared command endpoint. Each in-flight init write claims one transfer-request slot,
    # so cap the number of concurrent channel-init "lanes" at the configured pool size to
    # avoid exhausting it. No headroom is reserved: channel data flow and the RX/CMD
    # readers only start once every channel is initialised, so nothing else competes for
    # the pool during init.
    max_init_lanes = get_max_transfer_requests()
    type_by_name = {t.name: t for t in uart_types}

    for device in config:
        var = await register_usb_client(device)
        device_type = type_by_name.get(device[CONF_TYPE])
        if isinstance(device_type, MpxType):
            lanes = min(len(device[CONF_CHANNELS]), max_init_lanes)
            cg.add(var.set_init_lanes(lanes))
        for index, channel in enumerate(device[CONF_CHANNELS]):
            chvar = cg.new_Pvariable(channel[CONF_ID], index, channel[CONF_BUFFER_SIZE])
            await cg.register_parented(chvar, var)
            cg.add(chvar.set_stop_bits(channel[CONF_STOP_BITS]))
            cg.add(chvar.set_data_bits(channel[CONF_DATA_BITS]))
            cg.add(chvar.set_parity(channel[CONF_PARITY]))
            cg.add(chvar.set_baud_rate(channel[CONF_BAUD_RATE]))
            cg.add(chvar.set_dummy_receiver(channel[CONF_DUMMY_RECEIVER]))
            cg.add(chvar.set_flush_timeout(channel[CONF_FLUSH_TIMEOUT]))
            cg.add(chvar.set_debug(channel[CONF_DEBUG]))
            if channel[CONF_DEBUG_PREFIX]:
                cg.add(chvar.set_debug_prefix(channel[CONF_DEBUG_PREFIX]))
            cg.add(var.add_channel(chvar))
            if channel[CONF_DEBUG]:
                cg.add_define("USE_UART_DEBUGGER")
