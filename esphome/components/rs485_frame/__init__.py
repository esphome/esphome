from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_DELAY,
    CONF_INTERVAL,
    CONF_MODE,
    CONF_PAYLOAD,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_VALUE,
)
from esphome.core import HexInt
from esphome.types import ConfigType

CODEOWNERS = ["@b3nj1"]
DEPENDENCIES = ["uart"]
DOMAIN = "rs485_frame"
MULTI_CONF = True

# rs485_frame is not merged upstream, so these esphome.io URLs 404 until the docs PR lands too.
# Deliberate: a personal fork URL baked into a user-facing validation error is friction at merge
# review time and would need fixing then anyway, while the canonical URL is already correct for
# the PR's target state and needs no follow-up edit. The 404 window is short-lived and each
# message already states the fix inline without needing the link. `#framing-escape` and
# `#command-format` are stable anchors (`<span id="...">`) in the mdx source, not line numbers,
# so they do not drift as the doc is edited.
DOC_FRAMING_ESCAPE_URL = "https://esphome.io/components/rs485_frame/#framing-escape"
DOC_COMMAND_FORMAT_URL = "https://esphome.io/components/rs485_frame/#command-format"

rs485_frame_ns = cg.esphome_ns.namespace("rs485_frame")
RS485FrameHub = rs485_frame_ns.class_("RS485FrameHub", cg.Component, uart.UARTDevice)
RS485FrameTrigger = rs485_frame_ns.class_(
    "RS485FrameTrigger",
    automation.Trigger.template(
        cg.std_vector.template(cg.uint8).operator("const").operator("ref")
    ),
)

SensorDecode = rs485_frame_ns.enum("SensorDecode")
CrcVariant = rs485_frame_ns.enum("CrcVariant")
CrcType = rs485_frame_ns.enum("CrcType")
QueuePolicy = rs485_frame_ns.enum("QueuePolicy")
TxGateMode = rs485_frame_ns.enum("TxGateMode")

CONF_RS485_FRAME_ID = "rs485_frame_id"
CONF_ENDIAN = "endian"
CONF_COMMAND_FORMAT = "command_format"
CONF_VALUE_ELEMENT_BYTES = "value_element_bytes"
CONF_CRC = "crc"
CONF_DECODE = "decode"
CONF_DLE = "dle"
CONF_DUMP_FRAMES = "dump_frames"
CONF_ESCAPE = "escape"
CONF_BYTE = "byte"
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
CONF_ON_FRAME = "on_frame"
CONF_IDLE_COMMAND = "idle_command"
CONF_TX = "tx"
CONF_TX_VARIANT = "tx_variant"

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

# Diagnostic-only sensor decode types. User payload decoding is handled by on_frame:.
SENSOR_DECODES = {
    "frames_received": SensorDecode.SENSOR_DECODE_FRAMES_RECEIVED,
    "crc_failures": SensorDecode.SENSOR_DECODE_CRC_FAILURES,
    "commands_sent": SensorDecode.SENSOR_DECODE_COMMANDS_SENT,
    "command_drops": SensorDecode.SENSOR_DECODE_COMMAND_DROPS,
    "last_keepalive_ms": SensorDecode.SENSOR_DECODE_LAST_KEEPALIVE_MS,
    "queue_depth": SensorDecode.SENSOR_DECODE_QUEUE_DEPTH,
    # Frames abandoned mid-receive (max_frame_length overflow or intra-frame timeout) --
    # kept separate from crc_failures because these never reach CRC/structural validation
    # at all.
    "discarded_frames": SensorDecode.SENSOR_DECODE_DISCARDED_FRAMES,
}


# Schema caps for preamble / postamble byte lists. Must agree with the C++ StaticVector
# template arguments MAX_COMMAND_PREAMBLE_LEN / MAX_COMMAND_POSTAMBLE_LEN in rs485_frame.h.
MAX_COMMAND_PREAMBLE_LEN = 8
MAX_COMMAND_POSTAMBLE_LEN = 8

# Max values a button's `command:` may carry, serialised back-to-back in one frame. Must agree
# with MAX_COMMAND_VALUES in rs485_frame.h.
MAX_COMMAND_VALUES = 8


def validate_byte(value):
    value = cv.hex_uint8_t(value)
    return HexInt(int(value))


def validate_u32(value):
    value = cv.hex_uint32_t(value)
    return HexInt(int(value))


# Schema cap matches MAX_FRAME_TYPE_LEN in rs485_frame.h (StaticVector<uint8_t, 8>);
# the StaticVector::assign() truncates silently past N=8, so the schema must enforce the cap.
def validate_frame_type(value):
    return cv.All(cv.ensure_list(validate_byte), cv.Length(max=8))(value)


# Schema cap for the number of frame-type alternates per on_frame: entry. Must agree with
# MAX_FRAME_TYPE_ALTS in rs485_frame.h — the C++ StaticVector silently drops push_back past
# its cap, so the schema is the only place that surfaces "too many alternates" as an error.
MAX_FRAME_TYPE_ALTS = 4


# Used only for individual alternates inside the list-of-prefixes on_frame: form (see
# validate_frame_type_or_list below). Unlike the top-level single-prefix form, where an
# empty frame_type: [] is the documented match-all shorthand, an empty *alternate* inside
# a list (frame_type: [[0x01, 0x03], []]) has no such meaning and would silently make the
# whole trigger match-all -- so it requires at least one byte.
_validate_frame_type_alt = cv.All(
    cv.ensure_list(validate_byte), cv.Length(min=1, max=8)
)


def validate_frame_type_or_list(value):
    # on_frame: frame_type accepts either a single prefix ([0x01, 0x03]) or a list of
    # prefixes ([[0x01, 0x03], [0x01, 0x09]]) so one lambda can decode multiple related
    # frame types. Disambiguate by inspecting the first element: a list there means the
    # list-of-prefixes form, anything else (or empty) is the single-prefix form.
    # Normalize to a list-of-prefixes for code generation; the empty single-prefix form
    # (`frame_type: []` = match-all) is preserved as an empty prefix list so to_code emits
    # zero add_frame_type calls and the runtime falls back to its match-all branch.
    if value is None:
        return []
    if not isinstance(value, list):
        value = [value]
    if value and isinstance(value[0], list):
        return cv.All(
            cv.ensure_list(_validate_frame_type_alt),
            cv.Length(min=1, max=MAX_FRAME_TYPE_ALTS),
        )(value)
    single = validate_frame_type(value)
    if not single:
        return []
    return [single]


def _validate_command_format_bytes(max_len: int):
    return cv.All(cv.ensure_list(validate_byte), cv.Length(max=max_len))


# Schema for command_format: — defines how `value:` entries are serialised into the frame
# payload. A single data-driven block that can express any protocol variant without touching
# C++ (preamble bytes, per-element byte width, endianness, postamble bytes).
# command_format: is optional: hubs without it can only transmit via the raw `frame_type` +
# `payload` button form or the rs485_frame.send_frame action — the button platform's
# _final_validate rejects the `value:` shorthand against a hub that has none.
COMMAND_FORMAT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PREAMBLE, default=[]): _validate_command_format_bytes(
            MAX_COMMAND_PREAMBLE_LEN
        ),
        cv.Required(CONF_VALUE_ELEMENT_BYTES): cv.one_of(1, 2, 3, 4, int=True),
        cv.Optional(CONF_ENDIAN, default="big"): cv.one_of("big", "little", lower=True),
        cv.Optional(CONF_POSTAMBLE, default=[]): _validate_command_format_bytes(
            MAX_COMMAND_POSTAMBLE_LEN
        ),
    }
)


# Two ways a literal DLE inside the payload (or CRC) is byte-stuffed on the wire. There is
# no default: the wrong scheme silently corrupts every frame that happens to contain a DLE,
# and which one a bus uses is not inferable, so the user must declare it (mode: escape_byte
# is what Hayward uses; mode: double is the more common DLE-doubling convention).
ESCAPE_MODE_BYTE = "escape_byte"
ESCAPE_MODE_DOUBLE = "double"


def _validate_escape(value):
    value = ESCAPE_SCHEMA(value)
    mode = value[CONF_MODE]
    if mode == ESCAPE_MODE_BYTE and CONF_BYTE not in value:
        raise cv.Invalid(
            "framing.escape.byte is required when framing.escape.mode is escape_byte "
            "(the byte emitted after a DLE to mark it as literal data, e.g. 0x00)"
        )
    if mode == ESCAPE_MODE_DOUBLE and CONF_BYTE in value:
        raise cv.Invalid(
            "framing.escape.byte must be omitted when framing.escape.mode is double "
            "(a literal DLE is stuffed by doubling the DLE itself)"
        )
    return value


ESCAPE_SCHEMA = cv.Schema(
    {
        # No default: escape mode is a foot-gun, like the framing bytes, so require it.
        cv.Required(CONF_MODE): cv.one_of(
            ESCAPE_MODE_BYTE, ESCAPE_MODE_DOUBLE, lower=True
        ),
        cv.Optional(CONF_BYTE): validate_byte,
    }
)

FRAMING_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DLE, default=0x10): validate_byte,
        cv.Optional(CONF_STX, default=0x02): validate_byte,
        cv.Optional(CONF_ETX, default=0x03): validate_byte,
        cv.Optional(CONF_ESCAPE): _validate_escape,
    }
)

CRC_SCHEMA = cv.Schema(
    {
        # crc.type is required: there is no universally-correct default across DLE-framed
        # buses, so the user must declare the algorithm their device uses (use "none" to
        # accept any structurally-valid frame).
        cv.Required(CONF_TYPE): cv.one_of(*CRC_TYPES, lower=True),
        cv.Optional(
            CONF_RX_ACCEPT, default=["header_inclusive", "payload_only"]
        ): cv.ensure_list(cv.one_of(*CRC_VARIANTS, lower=True)),
        # tx_variant selects whether the DLE+STX header bytes participate in the transmitted
        # CRC. header_inclusive is the common case; set payload_only for buses that checksum
        # only the unescaped payload. It is a mechanical (not vendor) choice, so it defaults.
        cv.Optional(CONF_TX_VARIANT, default="header_inclusive"): cv.one_of(
            *CRC_VARIANTS, lower=True
        ),
    }
)

TX_GATE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MODE, default="frame_trigger"): cv.one_of(
            *TX_GATE_MODES, lower=True
        ),
        # No schema-level default for frame_type: it is device-specific (the bus keep-alive /
        # poll frame the hub transmits after). validate_hub() requires it for frame_trigger
        # mode; idle_gap and fixed_delay modes do not use it.
        cv.Optional(CONF_FRAME_TYPE): validate_frame_type,
        # delay=0 is valid (no delay after the gate frame before transmitting).
        cv.Optional(CONF_DELAY, default="0ms"): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_MIN_SILENCE, default="4ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_INTERVAL, default="100ms"
        ): cv.positive_time_period_milliseconds,
    }
)

# Upper bound for max_frame_length: protects against pathological YAML that would reserve
# ~4 KB scratch buffers per hub. ESPHome's largest framed protocols (Jandy iAqualinkTouch)
# stay well under 512 bytes; 1024 leaves ample headroom.
MAX_FRAME_LENGTH_UPPER = 1024
# Upper bound for max_queue_size with FIFO. Replace_latest is independently constrained to 1.
MAX_QUEUE_SIZE_UPPER = 32

TX_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_GATE, default={}): TX_GATE_SCHEMA,
        cv.Optional(CONF_QUEUE_POLICY, default="replace_latest"): cv.one_of(
            *QUEUE_POLICIES, lower=True
        ),
        # cv.positive_int allows 0, which would cause modulo-by-zero in the ring buffer.
        cv.Optional(CONF_MAX_QUEUE_SIZE, default=1): cv.int_range(
            min=1, max=MAX_QUEUE_SIZE_UPPER
        ),
        cv.Optional(CONF_IDLE_COMMAND): validate_u32,
    }
)


def validate_hub(config):
    # Framing-byte distinctness. The framer uses DLE as the escape introducer: in-frame,
    # DLE+STX starts a frame, DLE+ETX ends it, and DLE+escape_byte is a literal DLE. If any
    # of DLE/STX/ETX collide, or escape_byte equals STX or ETX, the byte stream is no longer
    # unambiguously parseable. Reject at config time rather than emitting unframeable data.
    framing = config[CONF_FRAMING]
    dle = framing[CONF_DLE]
    stx = framing[CONF_STX]
    etx = framing[CONF_ETX]
    if len({int(dle), int(stx), int(etx)}) != 3:
        raise cv.Invalid("framing dle, stx and etx must all be distinct byte values")
    # In escape_byte mode the marker must differ from stx/etx, otherwise an escaped DLE is
    # indistinguishable from a frame start/end terminator. In double mode the marker is the
    # DLE itself, which is already distinct from stx/etx by the check above.
    if (
        (escape := framing.get(CONF_ESCAPE)) is not None
        and escape[CONF_MODE] == ESCAPE_MODE_BYTE
        and int(escape[CONF_BYTE]) in (int(stx), int(etx))
    ):
        raise cv.Invalid(
            "framing.escape.byte must differ from stx and etx; otherwise an escaped DLE "
            "is indistinguishable from a frame start/end terminator"
        )

    if CONF_ESCAPE not in framing:
        raise cv.Invalid(
            "framing.escape is required but missing (set framing.escape.mode to escape_byte "
            f"or double); it has no default. See {DOC_FRAMING_ESCAPE_URL} for the two escape "
            "schemes"
        )
    if CONF_CRC not in config:
        raise cv.Invalid(
            "crc: is required (declare your device's checksum, or type: none)"
        )

    gate = config[CONF_TX][CONF_GATE]
    sniffer_only = config[CONF_SNIFFER_ONLY]
    gate_mode = gate[CONF_MODE]

    # In frame_trigger gate mode (non-sniffer), the gate frame type must be a non-empty byte
    # list — otherwise the gate would never fire and queued commands would accumulate
    # forever. It has no default, so require it explicitly.
    if (
        gate_mode == "frame_trigger"
        and not gate.get(CONF_FRAME_TYPE)
        and not sniffer_only
    ):
        raise cv.Invalid(
            "tx.gate.frame_type is required (a non-empty byte list) when tx.gate.mode is "
            "frame_trigger; the gate would never fire and queued commands would not transmit"
        )

    if (
        config[CONF_TX][CONF_QUEUE_POLICY] == "replace_latest"
        and config[CONF_TX][CONF_MAX_QUEUE_SIZE] != 1
    ):
        raise cv.Invalid("replace_latest requires max_queue_size: 1")

    # tx.idle_command is a uint32 that the hub serialises via command_format on every gate
    # with an empty queue. Without a command_format there is no defined encoding, so reject
    # the combination rather than silently emitting an undocumented default 4-byte big-endian
    # encoding (mirrors the button platform's `value:` rule).
    if CONF_IDLE_COMMAND in config[CONF_TX] and CONF_COMMAND_FORMAT not in config:
        raise cv.Invalid(
            "tx.idle_command requires a command_format: block on the hub (missing) so the "
            f"value has a defined on-wire encoding. See {DOC_COMMAND_FORMAT_URL}"
        )

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RS485FrameHub),
            cv.Optional(CONF_FRAMING, default={}): FRAMING_SCHEMA,
            # crc.type has no sensible cross-bus default; validate_hub() requires this block.
            cv.Optional(CONF_CRC): CRC_SCHEMA,
            cv.Optional(CONF_TX, default={}): TX_SCHEMA,
            cv.Optional(CONF_COMMAND_FORMAT): COMMAND_FORMAT_SCHEMA,
            cv.Optional(CONF_DUMP_FRAMES, default=False): cv.boolean,
            cv.Optional(CONF_SNIFFER_ONLY, default=False): cv.boolean,
            # Minimum legal RX frame = DLE STX FT0 FT1 DLE ETX = 6 bytes (no CRC).
            cv.Optional(CONF_MAX_FRAME_LENGTH, default=128): cv.int_range(
                min=6, max=MAX_FRAME_LENGTH_UPPER
            ),
            cv.Optional(
                CONF_FRAME_TIMEOUT, default="50ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_FRAME): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RS485FrameTrigger),
                    cv.Required(CONF_FRAME_TYPE): validate_frame_type_or_list,
                }
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_hub,
)


def _resolve_button_frame(
    button_config: ConfigType, hub_command_format: ConfigType | None = None
) -> list[int]:
    """Return the unframed bytes a button transmits.

    Mirrors RS485FrameHub::build_key_payload_ / queue_raw_frame's byte layout, so the button
    platform can check the frame against the hub's max_frame_length at validation time.
    """
    if CONF_VALUE in button_config:
        cf = button_config.get(CONF_COMMAND_FORMAT, hub_command_format)
        preamble = list(cf[CONF_PREAMBLE])
        postamble = list(cf[CONF_POSTAMBLE])
        element_bytes = button_config.get(
            CONF_VALUE_ELEMENT_BYTES, cf[CONF_VALUE_ELEMENT_BYTES]
        )
        big_endian = cf[CONF_ENDIAN] == "big"
        trigger = list(preamble)
        for value in button_config[CONF_VALUE]:
            value = int(value)
            byte_range = (
                range(element_bytes - 1, -1, -1) if big_endian else range(element_bytes)
            )
            trigger.extend((value >> (byte * 8)) & 0xFF for byte in byte_range)
        trigger.extend(postamble)
        return trigger
    # Raw form: frame_type + payload, concatenated verbatim (queue_raw_frame's layout).
    return list(button_config[CONF_FRAME_TYPE]) + list(button_config[CONF_PAYLOAD])


async def to_code(config):
    var = await uart.new_uart_component(config)

    framing = config[CONF_FRAMING]
    # The runtime tracks a single escape marker byte: in double mode a literal DLE is stuffed
    # as DLE DLE, so the marker is the DLE byte itself; in escape_byte mode it is the declared
    # byte. Resolving it here keeps the C++ framer/encoder a single code path.
    escape = framing[CONF_ESCAPE]
    if escape[CONF_MODE] == ESCAPE_MODE_DOUBLE:
        escape_marker = framing[CONF_DLE]
    else:
        escape_marker = escape[CONF_BYTE]
    cg.add(
        var.set_framing(
            framing[CONF_DLE],
            framing[CONF_STX],
            framing[CONF_ETX],
            escape_marker,
        )
    )

    crc = config[CONF_CRC]
    cg.add(var.set_crc_type(CRC_TYPES[crc[CONF_TYPE]]))
    cg.add(var.set_accept_header_crc("header_inclusive" in crc[CONF_RX_ACCEPT]))
    cg.add(var.set_accept_payload_crc("payload_only" in crc[CONF_RX_ACCEPT]))
    cg.add(var.set_tx_crc_variant(CRC_VARIANTS[crc[CONF_TX_VARIANT]]))

    tx = config[CONF_TX]
    gate = tx[CONF_GATE]
    # gate.frame_type is absent for idle_gap / fixed_delay modes (validate_hub only requires
    # it for frame_trigger); default to an empty list so the hub's gate matcher never fires.
    gate_frame_type = gate.get(CONF_FRAME_TYPE, [])
    cg.add(var.set_tx_gate_mode(TX_GATE_MODES[gate[CONF_MODE]]))
    cg.add(var.set_tx_gate_frame_type(gate_frame_type))
    cg.add(var.set_tx_gate_delay(gate[CONF_DELAY].total_milliseconds))
    cg.add(var.set_tx_idle_gap(gate[CONF_MIN_SILENCE].total_milliseconds))
    cg.add(var.set_tx_fixed_interval(gate[CONF_INTERVAL].total_milliseconds))
    cg.add(var.set_queue_policy(QUEUE_POLICIES[tx[CONF_QUEUE_POLICY]]))
    cg.add(var.set_max_queue_size(tx[CONF_MAX_QUEUE_SIZE]))
    if (idle_cmd := tx.get(CONF_IDLE_COMMAND)) is not None:
        cg.add(var.set_idle_command(idle_cmd))

    # command_format is only present when the user set it explicitly. Hubs without it can
    # still transmit via the raw button form or rs485_frame.send_frame — the button
    # platform's _final_validate rejects the `command:` shorthand against such a hub.
    if (cf := config.get(CONF_COMMAND_FORMAT)) is not None:
        cg.add(
            var.set_command_format(
                cf[CONF_PREAMBLE],
                cf[CONF_VALUE_ELEMENT_BYTES],
                cf[CONF_ENDIAN] == "big",
                cf[CONF_POSTAMBLE],
            )
        )
    cg.add(var.set_dump_frames(config[CONF_DUMP_FRAMES]))
    cg.add(var.set_sniffer_only(config[CONF_SNIFFER_ONLY]))
    cg.add(var.set_max_frame_length(config[CONF_MAX_FRAME_LENGTH]))
    cg.add(var.set_in_frame_timeout(config[CONF_FRAME_TIMEOUT].total_milliseconds))

    for conf in config.get(CONF_ON_FRAME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        # validate_frame_type_or_list normalizes the YAML to a list-of-prefixes; empty
        # list = match-all, in which case we emit zero add_frame_type calls and the
        # trigger's matches() falls through to its empty-list branch.
        for prefix in conf[CONF_FRAME_TYPE]:
            cg.add(trigger.add_frame_type(prefix))
        cg.add(var.register_trigger(trigger))
        await automation.build_automation(
            trigger,
            [
                (
                    cg.std_vector.template(cg.uint8).operator("const").operator("ref"),
                    "payload",
                )
            ],
            conf,
        )


# rs485_frame.send_frame: queue an arbitrary frame for transmission. Both frame_type
# and payload are templatable lists of bytes so callers can compute them at action time
# (e.g. from a trigger's payload, a global, or a sensor reading). The hub takes care of
# DLE-framing, byte-stuffing, and CRC according to the hub's crc.type and crc.tx_variant.
# This is the most flexible transmit path: it needs no command_format on the hub and can
# emit anything (probe frames, vendor-specific commands, or a value computed at runtime)
# that the `command:` button shorthand cannot express.
SEND_FRAME_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RS485FrameHub),
        cv.Required(CONF_FRAME_TYPE): cv.templatable(validate_frame_type),
        cv.Required(CONF_PAYLOAD): cv.templatable(cv.ensure_list(validate_byte)),
    }
)


automation.register_apply_action(
    "rs485_frame.send_frame",
    SEND_FRAME_SCHEMA,
    automation.ApplyCall(
        "queue_raw_frame({}, {})",
        (
            (CONF_FRAME_TYPE, cg.std_vector.template(cg.uint8)),
            (CONF_PAYLOAD, cg.std_vector.template(cg.uint8)),
        ),
    ),
)
