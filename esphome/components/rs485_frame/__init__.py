from dataclasses import dataclass, field

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.const import CONF_ON_FRAME
import esphome.config_validation as cv
from esphome.const import (
    CONF_DELAY,
    CONF_ID,
    CONF_INTERVAL,
    CONF_MODE,
    CONF_PAYLOAD,
    CONF_TYPE,
)
from esphome.core import CORE, ID, HexInt, Lambda
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

CODEOWNERS = ["@b3nj1"]
DEPENDENCIES = ["uart"]
DOMAIN = "rs485_frame"
MULTI_CONF = True

DOC_COMMAND_FORMAT_URL = "https://esphome.io/components/rs485_frame/#command-format"


@dataclass
class RS485FrameData:
    # Hub id and length of each constant send_frame, checked against the hub's max_frame_length
    send_frames: list[tuple[ID, int]] = field(default_factory=list)


def _data() -> RS485FrameData:
    return CORE.data.setdefault(DOMAIN, RS485FrameData())


rs485_frame_ns = cg.esphome_ns.namespace("rs485_frame")
RS485FrameHub = rs485_frame_ns.class_("RS485FrameHub", cg.Component, uart.UARTDevice)

CrcVariant = rs485_frame_ns.enum("CrcVariant")
CrcType = rs485_frame_ns.enum("CrcType")
QueuePolicy = rs485_frame_ns.enum("QueuePolicy")
TxGateMode = rs485_frame_ns.enum("TxGateMode")

CONF_RS485_FRAME_ID = "rs485_frame_id"
CONF_BYTE = "byte"
CONF_COMMAND_FORMAT = "command_format"
CONF_CRC = "crc"
CONF_DLE = "dle"
CONF_DUMP_FRAMES = "dump_frames"
CONF_ENDIAN = "endian"
CONF_ESCAPE = "escape"
CONF_ETX = "etx"
CONF_FRAME_TIMEOUT = "frame_timeout"
CONF_FRAME_TYPE = "frame_type"
CONF_FRAMING = "framing"
CONF_GATE = "gate"
CONF_MAX_FRAME_LENGTH = "max_frame_length"
CONF_MAX_QUEUE_SIZE = "max_queue_size"
CONF_MIN_SILENCE = "min_silence"
CONF_POSTAMBLE = "postamble"
CONF_PREAMBLE = "preamble"
CONF_QUEUE_POLICY = "queue_policy"
CONF_RX_ACCEPT = "rx_accept"
CONF_SNIFFER_ONLY = "sniffer_only"
CONF_STX = "stx"
CONF_TX = "tx"
CONF_TX_VARIANT = "tx_variant"
CONF_VALUE_ELEMENT_BYTES = "value_element_bytes"

CRC_VARIANTS = CrcVariant.options("header_inclusive", "payload_only", prefix="CRC_")
CRC_TYPES = CrcType.options(
    "none",
    "sum8",
    "sum16_big_endian",
    "sum16_little_endian",
    "xor8",
    "crc16_modbus",
    prefix="CRC_TYPE_",
)
QUEUE_POLICIES = QueuePolicy.options("replace_latest", "fifo", prefix="QUEUE_")
TX_GATE_MODES = TxGateMode.options(
    "frame_trigger", "idle_gap", "fixed_delay", prefix="TX_GATE_"
)

MAX_COMMAND_VALUES = 8
# Must agree with MAX_FRAME_TYPE_LEN in rs485_frame.h, which sizes a stack buffer.
MAX_FRAME_TYPE_LEN = 8
MAX_FRAME_TYPE_ALTS = 4
MAX_FRAME_LENGTH_UPPER = 1024
# Must agree with MIN_FRAME_LENGTH in rs485_frame.h.
MIN_FRAME_LENGTH = 2
MAX_QUEUE_SIZE_UPPER = 32
ESCAPE_MODE_BYTE = "escape_byte"
ESCAPE_MODE_DOUBLE = "double"


validate_value_element_bytes = cv.one_of(1, 2, 3, 4, int=True)


def _byte_list(min_len: int = 0) -> cv.All:
    return cv.All(
        cv.ensure_list(cv.hex_uint8_t), cv.Length(min=min_len, max=MAX_FRAME_TYPE_LEN)
    )


validate_frame_type = _byte_list()


@schema_extractor("schema")
def validate_frame_type_or_list(value: object) -> list[list[HexInt]]:
    """One prefix or a list of non-empty prefixes, as a list of prefixes; [] matches every frame."""
    if value is SCHEMA_EXTRACT:
        return validate_frame_type
    if value is None:
        raise cv.Invalid("frame_type needs a value; use [] to match every frame")
    if not isinstance(value, list):
        value = [value]
    if value and isinstance(value[0], list):
        return cv.All(
            cv.ensure_list(_byte_list(1)), cv.Length(min=1, max=MAX_FRAME_TYPE_ALTS)
        )(value)
    return [single] if (single := validate_frame_type(value)) else []


COMMAND_FORMAT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PREAMBLE, default=[]): _byte_list(),
        cv.Required(CONF_VALUE_ELEMENT_BYTES): validate_value_element_bytes,
        cv.Optional(CONF_ENDIAN, default="big"): cv.one_of("big", "little", lower=True),
        cv.Optional(CONF_POSTAMBLE, default=[]): _byte_list(),
    }
)


def check_frame_length(source: str, length: int, max_len: int) -> None:
    """Rejects a constant frame the hub would drop instead of sending."""
    if length < MIN_FRAME_LENGTH:
        raise cv.Invalid(
            f"{source} resolves to a {length}-byte frame; frames must be at least {MIN_FRAME_LENGTH} bytes"
        )
    if length > max_len:
        raise cv.Invalid(
            f"{source} resolves to a {length}-byte frame, over the hub's max_frame_length ({max_len})"
        )


def encode_command(
    values: list[int], command_format: ConfigType, element_bytes: int | None = None
) -> list[int]:
    """Preamble, each value as element_bytes in the format's byte order, then postamble."""
    size = element_bytes or command_format[CONF_VALUE_ELEMENT_BYTES]
    out = list(command_format[CONF_PREAMBLE])
    for value in values:
        if value >= 1 << (size * 8):
            raise cv.Invalid(f"value {value:#x} does not fit in {size} byte(s)")
        out.extend(int(value).to_bytes(size, command_format[CONF_ENDIAN]))
    return out + list(command_format[CONF_POSTAMBLE])


FRAMING_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DLE, default=0x10): cv.hex_uint8_t,
        cv.Optional(CONF_STX, default=0x02): cv.hex_uint8_t,
        cv.Optional(CONF_ETX, default=0x03): cv.hex_uint8_t,
        # No default: the wrong scheme silently corrupts every frame containing a DLE.
        cv.Required(CONF_ESCAPE): cv.typed_schema(
            {
                ESCAPE_MODE_BYTE: {cv.Required(CONF_BYTE): cv.hex_uint8_t},
                ESCAPE_MODE_DOUBLE: {},
            },
            key=CONF_MODE,
            lower=True,
        ),
    }
)

CRC_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TYPE): cv.one_of(*CRC_TYPES, lower=True),
        cv.Optional(
            CONF_RX_ACCEPT, default=["header_inclusive", "payload_only"]
        ): cv.All(
            cv.ensure_list(cv.one_of(*CRC_VARIANTS, lower=True)), cv.Length(min=1)
        ),
        cv.Optional(CONF_TX_VARIANT, default="header_inclusive"): cv.one_of(
            *CRC_VARIANTS, lower=True
        ),
    }
)

# Upper bound for every bus timing.
MAX_BUS_TIME_MS = 10000
_BUS_TIME = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(max=cv.TimePeriod(milliseconds=MAX_BUS_TIME_MS)),
)

_GATE_DELAY = {cv.Optional(CONF_DELAY, default="0ms"): _BUS_TIME}

TX_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_GATE, default={}): cv.typed_schema(
            {
                # frame_type is checked in validate_hub, since sniffer_only hubs may leave it out.
                "frame_trigger": {
                    cv.Optional(CONF_FRAME_TYPE): validate_frame_type,
                    **_GATE_DELAY,
                },
                "idle_gap": {
                    cv.Optional(CONF_MIN_SILENCE, default="4ms"): _BUS_TIME,
                    **_GATE_DELAY,
                },
                "fixed_delay": {
                    cv.Optional(CONF_INTERVAL, default="100ms"): _BUS_TIME,
                    **_GATE_DELAY,
                },
            },
            key=CONF_MODE,
            default_type="frame_trigger",
            lower=True,
        ),
        cv.Optional(CONF_QUEUE_POLICY, default="replace_latest"): cv.one_of(
            *QUEUE_POLICIES, lower=True
        ),
        cv.Optional(CONF_MAX_QUEUE_SIZE, default=1): cv.int_range(
            min=1, max=MAX_QUEUE_SIZE_UPPER
        ),
    }
)


def validate_hub(config: ConfigType) -> ConfigType:
    framing = config[CONF_FRAMING]
    stx, etx = int(framing[CONF_STX]), int(framing[CONF_ETX])
    if len({int(framing[CONF_DLE]), stx, etx}) != 3:
        raise cv.Invalid("framing dle, stx and etx must all be distinct byte values")
    if int(framing[CONF_ESCAPE].get(CONF_BYTE, framing[CONF_DLE])) in (stx, etx):
        raise cv.Invalid("framing.escape.byte must differ from stx and etx")
    tx = config[CONF_TX]
    gate = tx[CONF_GATE]
    if gate[CONF_MODE] == "frame_trigger" and not config[CONF_SNIFFER_ONLY]:
        frame_type = gate.get(CONF_FRAME_TYPE)
        if not frame_type:
            raise cv.Invalid("tx.gate.frame_type is required for frame_trigger")
        if len(frame_type) > config[CONF_MAX_FRAME_LENGTH]:
            raise cv.Invalid(
                f"tx.gate.frame_type is longer than max_frame_length ({config[CONF_MAX_FRAME_LENGTH]})"
            )
    if tx[CONF_QUEUE_POLICY] == "replace_latest" and tx[CONF_MAX_QUEUE_SIZE] != 1:
        raise cv.Invalid("replace_latest requires max_queue_size: 1")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RS485FrameHub),
            cv.Required(CONF_FRAMING): FRAMING_SCHEMA,
            cv.Required(CONF_CRC): CRC_SCHEMA,
            cv.Optional(CONF_TX, default={}): TX_SCHEMA,
            cv.Optional(CONF_COMMAND_FORMAT): COMMAND_FORMAT_SCHEMA,
            cv.Optional(CONF_DUMP_FRAMES, default=False): cv.boolean,
            cv.Optional(CONF_SNIFFER_ONLY, default=False): cv.boolean,
            # Counts frame_type + payload, before escaping and without the CRC.
            cv.Optional(CONF_MAX_FRAME_LENGTH, default=128): cv.int_range(
                min=MIN_FRAME_LENGTH, max=MAX_FRAME_LENGTH_UPPER
            ),
            cv.Optional(CONF_FRAME_TIMEOUT, default="50ms"): _BUS_TIME,
            cv.Optional(CONF_ON_FRAME): automation.validate_synchronous_automation(
                "rs485_frame on_frame",
                "payload",
                extra_schema={
                    cv.Required(CONF_FRAME_TYPE): validate_frame_type_or_list
                },
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_hub,
)


def _crc_len(config: ConfigType) -> int:
    return {"none": 0, "sum8": 1, "xor8": 1}.get(config[CONF_CRC][CONF_TYPE], 2)


async def to_code(config: ConfigType) -> None:
    hub_id = config[CONF_ID]
    framing = config[CONF_FRAMING]
    crc = config[CONF_CRC]
    tx_config = config[CONF_TX]
    crc_len = _crc_len(config)
    max_queue_size = tx_config[CONF_MAX_QUEUE_SIZE]
    frame_size = config[CONF_MAX_FRAME_LENGTH] + crc_len
    rx = cg.static_buffer(hub_id, cg.uint8, frame_size)
    tx = tx_len = cg.nullptr
    if not (sniffer_only := config[CONF_SNIFFER_ONLY]):
        tx = cg.static_buffer(hub_id, cg.uint8, max_queue_size * frame_size)
        tx_len = cg.static_buffer(hub_id, cg.uint16, max_queue_size)
    var = await uart.new_uart_component(
        config,
        framing[CONF_DLE],
        framing[CONF_STX],
        framing[CONF_ETX],
        framing[CONF_ESCAPE].get(CONF_BYTE, framing[CONF_DLE]),
        CRC_TYPES[crc[CONF_TYPE]],
        config[CONF_MAX_FRAME_LENGTH],
        max_queue_size,
        rx,
        tx,
        tx_len,
    )

    # Setters are only emitted when the value differs from the C++ default.
    if "header_inclusive" not in crc[CONF_RX_ACCEPT]:
        cg.add(var.set_accept_header_crc(False))
    if "payload_only" not in crc[CONF_RX_ACCEPT]:
        cg.add(var.set_accept_payload_crc(False))
    if config[CONF_DUMP_FRAMES]:
        cg.add(var.set_dump_frames(True))
    if (timeout := config[CONF_FRAME_TIMEOUT].total_milliseconds) != 50:
        cg.add(var.set_in_frame_timeout(timeout))
    if not sniffer_only:
        gate = tx_config[CONF_GATE]
        if crc[CONF_TX_VARIANT] != "header_inclusive":
            cg.add(var.set_tx_crc_variant(CRC_VARIANTS[crc[CONF_TX_VARIANT]]))
        if gate[CONF_MODE] != "frame_trigger":
            cg.add(var.set_tx_gate_mode(TX_GATE_MODES[gate[CONF_MODE]]))
        if gate_frame_type := gate.get(CONF_FRAME_TYPE):
            table = automation.progmem_bytes(f"{hub_id}_frame_type", gate_frame_type)
            cg.add(var.set_tx_gate_frame_type(table, len(gate_frame_type)))
        if delay := gate[CONF_DELAY].total_milliseconds:
            cg.add(var.set_tx_gate_delay(delay))
        interval = gate.get(CONF_MIN_SILENCE, gate.get(CONF_INTERVAL))
        if interval is not None:
            cg.add(var.set_tx_gate_interval(interval.total_milliseconds))
        if tx_config[CONF_QUEUE_POLICY] != "replace_latest":
            cg.add(var.set_queue_policy(QUEUE_POLICIES[tx_config[CONF_QUEUE_POLICY]]))

    for conf in config.get(CONF_ON_FRAME, []):
        checks = [
            f"rs485_frame::frame_has_prefix(payload, "
            f"{automation.progmem_bytes(f'{hub_id}_frame_type', prefix)}, {len(prefix)})"
            for prefix in conf[CONF_FRAME_TYPE]
        ]
        await automation.build_callback_automation(
            var,
            "add_on_frame_callback",
            [(cg.byte_span, "payload")],
            conf,
            params=[(cg.byte_span, "payload")],
            when=" || ".join(checks) or None,
        )


def _progmem_span(config: ConfigType, value: list[int]) -> str:
    if not value:
        return "{}"
    table = automation.progmem_bytes(f"{config[CONF_ID]}_send", value)
    return str(cg.byte_span(table, len(value)))


def _validate_send_frame(config: ConfigType) -> ConfigType:
    parts = (config[CONF_FRAME_TYPE], config[CONF_PAYLOAD])
    if not any(isinstance(part, Lambda) for part in parts):
        _data().send_frames.append((config[CONF_ID], sum(map(len, parts))))
    return config


def _final_validate(config: ConfigType) -> ConfigType:
    # Each hub needs its UART's RX pin to itself, and its TX pin unless sniffer_only.
    uart.final_validate_device_schema(
        f"rs485_frame {config[CONF_ID].id}",
        require_rx=True,
        require_tx=not config[CONF_SNIFFER_ONLY],
    )(config)
    for hub_id, length in _data().send_frames:
        if hub_id.id == config[CONF_ID].id:
            check_frame_length(
                "rs485_frame.send_frame", length, config[CONF_MAX_FRAME_LENGTH]
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


automation.register_apply_action(
    "rs485_frame.send_frame",
    cv.All(
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(RS485FrameHub),
                cv.Required(CONF_FRAME_TYPE): cv.templatable(validate_frame_type),
                cv.Required(CONF_PAYLOAD): cv.templatable(
                    cv.ensure_list(cv.hex_uint8_t)
                ),
            }
        ),
        _validate_send_frame,
    ),
    automation.ApplyCall(
        "queue_raw_frame({}, {})",
        (
            (CONF_FRAME_TYPE, cg.std_vector.template(cg.uint8), _progmem_span),
            (CONF_PAYLOAD, cg.std_vector.template(cg.uint8), _progmem_span),
        ),
    ),
)
