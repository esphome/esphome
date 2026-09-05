from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTOMATION_ID,
    CONF_BUTTON,
    CONF_DELAY,
    CONF_DISCOVERY,
    CONF_ID,
    CONF_INTERVAL,
    CONF_LENGTH,
    CONF_MODE,
    CONF_NAME,
    CONF_OFFSET,
    CONF_PAYLOAD,
    CONF_SIZE,
    CONF_THEN,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_VALUE,
)
from esphome.core import ID as CoreID, HexInt
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@b3nj1"]
DEPENDENCIES = ["uart"]
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
SendFrameAction = rs485_frame_ns.class_("SendFrameAction", automation.Action)
DumpFrameTraceAction = rs485_frame_ns.class_("DumpFrameTraceAction", automation.Action)

SensorDecode = rs485_frame_ns.enum("SensorDecode")
CrcVariant = rs485_frame_ns.enum("CrcVariant")
CrcType = rs485_frame_ns.enum("CrcType")
QueuePolicy = rs485_frame_ns.enum("QueuePolicy")
TxGateMode = rs485_frame_ns.enum("TxGateMode")

CONF_RS485_FRAME_ID = "rs485_frame_id"
CONF_ASCII_STRIP_HIGH_BIT = "ascii_strip_high_bit"
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
CONF_IDLE_GAP = "idle_gap"
CONF_FRAME_TIMEOUT = "frame_timeout"
CONF_FRAME_TYPE = "frame_type"
CONF_FRAMING = "framing"
CONF_GATE = "gate"
CONF_MAX_FRAME_LENGTH = "max_frame_length"
CONF_MAX_FRAME_TYPES = "max_frame_types"
CONF_MIN_FRAMING_CONFIDENCE = "min_framing_confidence"
CONF_BAUD_SWEEP = "baud_sweep"
CONF_DATA_BITS_SWEEP = "data_bits_sweep"
CONF_DWELL = "dwell"
CONF_MAX_QUEUE_SIZE = "max_queue_size"
CONF_MAX_UNIQUE_PAYLOADS = "max_unique_payloads"
CONF_MIN_SILENCE = "min_silence"
CONF_PAYLOAD_CAPTURE_BYTES = "payload_capture_bytes"
CONF_PAYLOAD_DUMP_TOP = "payload_dump_top"
CONF_POSTAMBLE = "postamble"
CONF_PREAMBLE = "preamble"
CONF_QUEUE_POLICY = "queue_policy"
CONF_REFERENCE_FRAME_TYPE = "reference_frame_type"
CONF_REFERENCE_MODE = "reference_mode"
CONF_RX_ACCEPT = "rx_accept"
CONF_SNIFFER_ONLY = "sniffer_only"
CONF_SNIFFER_STATS = "sniffer_stats"
CONF_STX = "stx"
CONF_ON_FRAME = "on_frame"
CONF_IDLE_COMMAND = "idle_command"
CONF_TX = "tx"
CONF_TX_VARIANT = "tx_variant"

# response_fields: / response_monitor: — live trigger -> response confirmation.
CONF_RESPONSE_FIELDS = "response_fields"
CONF_RESPONSE_MONITOR = "response_monitor"
CONF_FRAME_TYPE_MASK = "frame_type_mask"
CONF_TRIGGER = "trigger"
CONF_BUTTON_ID = "button_id"
CONF_BUTTON_NAME = "button_name"
CONF_WINDOW = "window"
CONF_SIGNATURE = "signature"
CONF_FIELD = "field"
CONF_MASK = "mask"
CONF_BIT = "bit"
CONF_VALUES = "values"
CONF_ON_CONFIRMED = "on_confirmed"
CONF_ON_FAILED = "on_failed"

# frame_trace: — a raw RX/TX ring buffer for response_monitor diagnostics.
CONF_FRAME_TRACE = "frame_trace"
CONF_CAPTURE_BYTES = "capture_bytes"
CONF_INCLUDE_INVALID = "include_invalid"

CRC_VARIANTS = {
    "header_inclusive": CrcVariant.CRC_HEADER_INCLUSIVE,
    "payload_only": CrcVariant.CRC_PAYLOAD_ONLY,
}

CRC_TYPES = {
    "none": CrcType.CRC_TYPE_NONE,
    "sum8": CrcType.CRC_TYPE_SUM8,
    "sum16_big_endian": CrcType.CRC_TYPE_SUM16_BIG_ENDIAN,
    "sum16_little_endian": CrcType.CRC_TYPE_SUM16_LITTLE_ENDIAN,
    "xor8": CrcType.CRC_TYPE_XOR8,
    "crc16_modbus": CrcType.CRC_TYPE_CRC16_MODBUS,
}

QUEUE_POLICIES = {
    "replace_latest": QueuePolicy.QUEUE_REPLACE_LATEST,
    "fifo": QueuePolicy.QUEUE_FIFO,
}

TX_GATE_MODES = {
    "frame_trigger": TxGateMode.TX_GATE_FRAME_TRIGGER,
    "idle_gap": TxGateMode.TX_GATE_IDLE_GAP,
    "fixed_delay": TxGateMode.TX_GATE_FIXED_DELAY,
}

# Diagnostic-only sensor decode types. User payload decoding is handled by on_frame:.
SENSOR_DECODES = {
    "frames_received": SensorDecode.SENSOR_DECODE_FRAMES_RECEIVED,
    "crc_failures": SensorDecode.SENSOR_DECODE_CRC_FAILURES,
    "commands_sent": SensorDecode.SENSOR_DECODE_COMMANDS_SENT,
    "command_drops": SensorDecode.SENSOR_DECODE_COMMAND_DROPS,
    "last_keepalive_ms": SensorDecode.SENSOR_DECODE_LAST_KEEPALIVE_MS,
    "queue_depth": SensorDecode.SENSOR_DECODE_QUEUE_DEPTH,
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
            cv.ensure_list(validate_frame_type),
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

# escape is optional at the schema level so a discovery: hub (which does not yet know the
# scheme) need not declare it. validate_hub() requires it for every non-discovery hub.
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
        # accept any structurally-valid frame, e.g. while discovering an unknown bus).
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
# Upper bound for sniffer_stats max_frame_types. Must agree with SNIFFER_MAX_FRAME_TYPES_UPPER
# in sniffer_stats.h — the C++ side caps the FixedVector capacity at that constant, so a
# larger schema value would silently truncate.
SNIFFER_MAX_FRAME_TYPES_UPPER = 64

# Upper bound for sniffer_stats max_unique_payloads. Per-entry heap allocation grows linearly
# in this value; 64 is a comfortable ceiling for "lots of distinct display screens" without
# making it easy to OOM an ESP8266 via a typo.
SNIFFER_MAX_UNIQUE_PAYLOADS_UPPER = 64

# Upper bound for sniffer_stats payload_capture_bytes. PayloadCapture::len is uint8_t, so
# 256 wraps to 0 — cap at 255. A frame longer than this is still uniquely identified by its
# first 255 bytes; collisions on the first 255 bytes are astronomically unlikely.
SNIFFER_PAYLOAD_CAPTURE_BYTES_UPPER = 255

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

# reference_mode: receive (default) keeps d-ref measuring since the last RX frame matching
# reference_frame_type, unchanged from before this field existed. send measures d-ref since
# the last TX event instead — "how long after our own send did frame X arrive" — and ignores
# reference_frame_type. Passed to C++ as a bool (reference_mode_send), matching the existing
# ascii_strip_high_bit pattern rather than adding enum codegen plumbing for a two-value field.
SNIFFER_REFERENCE_MODES = ["receive", "send"]

# Schema for sniffer_stats: — an optional diagnostic that buckets RX frames by frame_type
# and logs cadence + unique-payload histograms on a periodic interval. Compiled out unless
# the YAML block is present (see USE_RS485_FRAME_SNIFFER_STATS in sniffer_stats.h).
SNIFFER_STATS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_INTERVAL, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_FRAME_TYPES, default=32): cv.int_range(
            min=1, max=SNIFFER_MAX_FRAME_TYPES_UPPER
        ),
        # Per-frame-type unique-payload capacity. Bumped from the original 8 to 16 so
        # protocols with many display screens or per-button responses don't fill the
        # bucket immediately and start counting everything into +overflow.
        cv.Optional(CONF_MAX_UNIQUE_PAYLOADS, default=16): cv.int_range(
            min=1, max=SNIFFER_MAX_UNIQUE_PAYLOADS_UPPER
        ),
        # How many payload bytes are captured per unique sample. 32 covers most decode
        # use cases including Hayward display frames; bump for AquaLogic / iAqualinkTouch
        # variants that ship longer frames. Capped at SNIFFER_PAYLOAD_CAPTURE_BYTES_UPPER.
        cv.Optional(CONF_PAYLOAD_CAPTURE_BYTES, default=32): cv.int_range(
            min=1, max=SNIFFER_PAYLOAD_CAPTURE_BYTES_UPPER
        ),
        # payload_dump_top=0 disables the hex/ASCII dump that follows the table; only the
        # summary row per frame_type is logged in that case. Capped at MAX_QUEUE_SIZE_UPPER
        # (32) just to keep log volume bounded — there is no inherent upper limit.
        cv.Optional(CONF_PAYLOAD_DUMP_TOP, default=0): cv.int_range(min=0, max=32),
        # reference_frame_type defaults to the active tx.gate.frame_type (typically the
        # bus keep-alive) when omitted; supplied here when you want d-ref measured against
        # something other than the gate.
        cv.Optional(CONF_REFERENCE_FRAME_TYPE): validate_frame_type,
        # See SNIFFER_REFERENCE_MODES above. No behavior change for existing configs — receive
        # reproduces the pre-existing reference_frame_type-only behavior exactly.
        cv.Optional(CONF_REFERENCE_MODE, default="receive"): cv.one_of(
            *SNIFFER_REFERENCE_MODES, lower=True
        ),
        # Off by default so the preview shows raw byte values on binary buses where the high
        # bit carries data. Enable for display-frame buses that pack an attribute flag (e.g.
        # blink/inverse) into bit 7, so the underlying character renders instead of '.'.
        cv.Optional(CONF_ASCII_STRIP_HIGH_BIT, default=False): cv.boolean,
    }
)


# Upper bound for frame_trace.size (ring buffer capacity, in frames). Must agree with
# FRAME_TRACE_SIZE_UPPER in frame_trace.h — the C++ side caps the FixedVector capacity at
# that constant, so a larger schema value would silently truncate.
FRAME_TRACE_SIZE_UPPER = 128

# Upper bound for frame_trace.capture_bytes. FrameTraceEntry::captured_len is uint8_t-backed,
# so 256 would wrap to 0 — cap at 255, same reasoning as SNIFFER_PAYLOAD_CAPTURE_BYTES_UPPER.
FRAME_TRACE_CAPTURE_BYTES_UPPER = 255

# Schema for frame_trace: — a fixed-size ring buffer of recent RX/TX frames (raw on-wire
# bytes, still escape-stuffed), dumped to the log via rs485_frame.dump_frame_trace. Compiled
# out unless the block is present (see USE_RS485_FRAME_FRAME_TRACE in frame_trace.h).
FRAME_TRACE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SIZE, default=32): cv.int_range(
            min=1, max=FRAME_TRACE_SIZE_UPPER
        ),
        cv.Optional(CONF_CAPTURE_BYTES, default=24): cv.int_range(
            min=1, max=FRAME_TRACE_CAPTURE_BYTES_UPPER
        ),
        # Also capture frames that fail CRC/framing, not just frames that pass
        # validate_frame_(). This is the one thing frame_trace: needs that
        # sniffer_stats/response_monitor don't — those only ever see already-validated frames.
        cv.Optional(CONF_INCLUDE_INVALID, default=True): cv.boolean,
    }
)


# Schema for discovery: — a passive framing/CRC reverse-engineering aid for an unknown bus.
# When present the hub does no framing, validation, or transmission; it captures raw bytes,
# segments them by idle gap, and logs candidate framing bytes, escape scheme, and CRC scheme.
# Compiled out unless the block is present (see USE_RS485_FRAME_DISCOVERY in discovery.h).
DISCOVERY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_INTERVAL, default="30s"): cv.positive_time_period_milliseconds,
        # Idle time that ends a burst (frame). The classic rule of thumb is ~3 character times;
        # 5ms covers 9600-19200 baud and stays above the loop cadence so frames are not split.
        # The segmenter resolves gaps only at loop granularity, so very tightly packed frames
        # may merge — raise this only if bursts are being split mid-frame.
        cv.Optional(CONF_IDLE_GAP, default="5ms"): cv.positive_time_period_milliseconds,
        # Minimum share (percent) of voting bursts that must agree on the top start/end delimiter
        # pair before discovery reports the framing as confident and prints a ready-to-paste
        # config. A real DLE-framed bus sits near 100%; a non-DLE or noisy bus splits its votes
        # and stays low. Set to 0 to always print the best guess. CRC scoring is unaffected.
        cv.Optional(CONF_MIN_FRAMING_CONFIDENCE, default=80): cv.int_range(
            min=0, max=100
        ),
        # baud_sweep: when present, discovery cycles the UART through each baud rate (crossed with
        # data_bits_sweep) for `dwell`, scores the framing at each, and locks onto the best before
        # continuing. Omit it if you already know the baud rate. Runtime UART reconfiguration is
        # implemented on ESP-IDF and ESP8266; on other platforms the sweep cannot change settings.
        cv.Optional(CONF_BAUD_SWEEP): cv.All(
            cv.ensure_list(cv.int_range(min=300, max=2000000)), cv.Length(min=1)
        ),
        # data_bits widths to try at each baud. RS485 is almost always 8; 7 is the only other
        # value worth trying on legacy buses. Crossed with baud_sweep to form the candidate list.
        cv.Optional(CONF_DATA_BITS_SWEEP, default=[8]): cv.All(
            cv.ensure_list(cv.int_range(min=5, max=8)), cv.Length(min=1)
        ),
        # Time spent capturing at each candidate before scoring it. Needs enough traffic for the
        # framing to converge; 10s suits a bus with a steady keep-alive. Raise it for sparse buses.
        cv.Optional(CONF_DWELL, default="10s"): cv.positive_time_period_milliseconds,
    }
)


# response_fields:/response_monitor: schema caps. Must agree with the matching C++ constants
# in response_monitor.h (MAX_RESPONSE_FIELD_PREFIX_LEN, MAX_RESPONSE_FIELD_LEN,
# MAX_RESPONSE_FIELDS, MAX_RESPONSE_TRIGGER_LEN, MAX_RESPONSE_MONITOR_ENTRIES,
# MAX_SIGNATURE_ALTS, MAX_MASKED_INT_VALUES, MAX_TEXT_ENUM_VALUES, MAX_TEXT_ENUM_LEN).
MAX_RESPONSE_FIELD_PREFIX_LEN = 16
MAX_RESPONSE_FIELD_LEN = 40
MAX_RESPONSE_FIELDS = 16
MAX_RESPONSE_TRIGGER_LEN = 48
MAX_RESPONSE_MONITOR_ENTRIES = 16
MAX_SIGNATURE_ALTS = 4
MAX_MASKED_INT_VALUES = 4
MAX_TEXT_ENUM_VALUES = 8
MAX_TEXT_ENUM_LEN = 32

SIGNATURE_TYPE_MASKED_INT = "masked_int"
SIGNATURE_TYPE_TEXT_ENUM = "text_enum"
SIGNATURE_TYPE_CHANGED = "changed"
SIGNATURE_TYPE_CHANGED_GATED = "changed_gated"
SIGNATURE_TYPES = [
    SIGNATURE_TYPE_MASKED_INT,
    SIGNATURE_TYPE_TEXT_ENUM,
    SIGNATURE_TYPE_CHANGED,
    SIGNATURE_TYPE_CHANGED_GATED,
]


def _validate_response_field_prefix(value):
    return cv.All(
        cv.ensure_list(validate_byte), cv.Length(max=MAX_RESPONSE_FIELD_PREFIX_LEN)
    )(value)


def _validate_response_field(value):
    value = cv.Schema(
        {
            cv.Required(CONF_FRAME_TYPE): _validate_response_field_prefix,
            cv.Optional(CONF_FRAME_TYPE_MASK): _validate_response_field_prefix,
            cv.Required(CONF_OFFSET): cv.int_range(min=0, max=255),
            cv.Required(CONF_LENGTH): cv.int_range(min=1, max=MAX_RESPONSE_FIELD_LEN),
            cv.Optional(CONF_ENDIAN, default="big"): cv.one_of(
                "big", "little", lower=True
            ),
        }
    )(value)
    mask = value.get(CONF_FRAME_TYPE_MASK)
    if mask is not None and len(mask) != len(value[CONF_FRAME_TYPE]):
        raise cv.Invalid(
            "response_fields entry: frame_type_mask must be the same length as frame_type "
            f"({len(value[CONF_FRAME_TYPE])} bytes)"
        )
    return value


def _validate_response_fields(value):
    value = cv.Schema({cv.string: _validate_response_field})(value)
    if len(value) > MAX_RESPONSE_FIELDS:
        raise cv.Invalid(
            f"response_fields: supports at most {MAX_RESPONSE_FIELDS} entries, got {len(value)}"
        )
    return value


def _validate_button_id(value):
    # Deliberately untyped (unlike cv.use_id(SomeType)): this component's own button
    # sub-platform package is named "button", so `from esphome.components import button`
    # at module scope here resolves to this file's own .button subpackage rather than the
    # core button component under this loader's import scheme — importing the core button
    # component to type-check against is unsafe from this file.
    # cv.use_id's "type" argument is metadata only (see config_validation.py's use_id()); the
    # actual cross-reference is by id name, resolved and checked in FINAL_VALIDATE_SCHEMA.
    cv.check_not_templatable(value)
    return CoreID(cv.validate_id_name(value), is_declaration=False, type=None)


def _validate_trigger(value):
    value = cv.Schema(
        {
            cv.Optional(CONF_BUTTON_ID): _validate_button_id,
            cv.Optional(CONF_BUTTON_NAME): cv.string_strict,
            cv.Optional(CONF_FRAME_TYPE): validate_frame_type,
        }
    )(value)
    cv.has_exactly_one_key(CONF_BUTTON_ID, CONF_BUTTON_NAME, CONF_FRAME_TYPE)(value)
    return value


# `mask:` and `bit:` are two representations of the same signature-matching value: `bit: N` is
# shorthand for `mask: 0x<1<<N>`, for the common case of watching a single bit (e.g. one LED in a
# bitmask field also exposed per-bit by led.yaml) without the config author computing the hex mask
# by hand and keeping it in sync with the bit position used elsewhere. Mutually exclusive; resolved
# to CONF_MASK here so every downstream consumer (codegen) only ever sees CONF_MASK.
def _resolve_bit_to_mask(value):
    if CONF_BIT in value:
        value = {**value, CONF_MASK: HexInt(1 << value[CONF_BIT])}
        del value[CONF_BIT]
    elif CONF_MASK not in value:
        value = {**value, CONF_MASK: HexInt(0xFFFFFFFF)}
    return value


MASKED_INT_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_FIELD): cv.string,
            cv.Required(CONF_TYPE): cv.one_of(SIGNATURE_TYPE_MASKED_INT),
            cv.Optional(CONF_MASK): validate_u32,
            cv.Optional(CONF_BIT): cv.int_range(min=0, max=31),
            cv.Required(CONF_VALUE): cv.All(
                cv.ensure_list(validate_u32),
                cv.Length(min=1, max=MAX_MASKED_INT_VALUES),
            ),
        }
    ),
    cv.has_at_most_one_key(CONF_MASK, CONF_BIT),
    _resolve_bit_to_mask,
)

TEXT_ENUM_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FIELD): cv.string,
        cv.Required(CONF_TYPE): cv.one_of(SIGNATURE_TYPE_TEXT_ENUM),
        cv.Required(CONF_VALUES): cv.All(
            cv.ensure_list(cv.All(cv.string_strict, cv.Length(max=MAX_TEXT_ENUM_LEN))),
            cv.Length(min=1, max=MAX_TEXT_ENUM_VALUES),
        ),
    }
)

CHANGED_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_FIELD): cv.string,
            cv.Required(CONF_TYPE): cv.one_of(SIGNATURE_TYPE_CHANGED),
            cv.Optional(CONF_MASK): validate_u32,
            cv.Optional(CONF_BIT): cv.int_range(min=0, max=31),
        }
    ),
    cv.has_at_most_one_key(CONF_MASK, CONF_BIT),
    _resolve_bit_to_mask,
)

# changed_gated's gate: a second, independent field read at trigger time. Reuses CONF_GATE
# (already "gate") from tx.gate.
GATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_FIELD): cv.string,
            cv.Optional(CONF_MASK): validate_u32,
            cv.Optional(CONF_BIT): cv.int_range(min=0, max=31),
            cv.Required(CONF_VALUE): validate_u32,
        }
    ),
    cv.has_at_most_one_key(CONF_MASK, CONF_BIT),
    _resolve_bit_to_mask,
)

CHANGED_GATED_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_FIELD): cv.string,
            cv.Required(CONF_TYPE): cv.one_of(SIGNATURE_TYPE_CHANGED_GATED),
            cv.Optional(CONF_MASK): validate_u32,
            cv.Optional(CONF_BIT): cv.int_range(min=0, max=31),
            cv.Required(CONF_GATE): GATE_SCHEMA,
        }
    ),
    cv.has_at_most_one_key(CONF_MASK, CONF_BIT),
    _resolve_bit_to_mask,
)

_SIGNATURE_ALT_SCHEMAS = {
    SIGNATURE_TYPE_MASKED_INT: MASKED_INT_SCHEMA,
    SIGNATURE_TYPE_TEXT_ENUM: TEXT_ENUM_SCHEMA,
    SIGNATURE_TYPE_CHANGED: CHANGED_SCHEMA,
    SIGNATURE_TYPE_CHANGED_GATED: CHANGED_GATED_SCHEMA,
}


def _validate_signature_alt(value):
    # The type: key determines which of the four mode schemas applies; peek at it first
    # (with cv.ALLOW_EXTRA so the type-specific keys don't fail this preliminary pass)
    # before dispatching to the schema that actually validates every key.
    peek = cv.Schema(
        {cv.Required(CONF_TYPE): cv.one_of(*SIGNATURE_TYPES, lower=True)},
        extra=cv.ALLOW_EXTRA,
    )(value)
    return _SIGNATURE_ALT_SCHEMAS[peek[CONF_TYPE]](value)


RESPONSE_MONITOR_ENTRY_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.string,
        cv.Required(CONF_TRIGGER): _validate_trigger,
        # No default: like framing.escape and crc.type, a wrong guess here silently breaks
        # the feature (too short: real presses reported as failures; too long: unrelated
        # later traffic wrongly credited as a confirmation).
        cv.Required(CONF_WINDOW): cv.positive_time_period_milliseconds,
        cv.Required(CONF_SIGNATURE): cv.All(
            cv.ensure_list(_validate_signature_alt),
            cv.Length(min=1, max=MAX_SIGNATURE_ALTS),
        ),
        # Fire on this entry's own SUCCESS (on_confirmed:) or FAIL/TIMEOUT (on_failed:)
        # outcomes only -- never on NOT_APPLICABLE (an expected "gate didn't hold this time",
        # not a failure) or ORPHAN (no trigger from this entry was even pending). No args:
        # callers that need to know which button confirmed/failed already know, since each
        # entry names exactly one trigger. validate_automation({}) still generates a
        # CONF_TRIGGER_ID (AUTOMATION_SCHEMA always declares one); this codegen path just
        # never instantiates it, the same way automation.build_callback_automation already
        # leaves it unused for triggers like on_press.
        cv.Optional(CONF_ON_CONFIRMED): automation.validate_automation({}),
        cv.Optional(CONF_ON_FAILED): automation.validate_automation({}),
    }
)


def _validate_response_monitor(value):
    value = cv.All(
        cv.ensure_list(RESPONSE_MONITOR_ENTRY_SCHEMA),
        cv.Length(max=MAX_RESPONSE_MONITOR_ENTRIES),
    )(value)
    names = [entry[CONF_NAME] for entry in value]
    if len(names) != len(set(names)):
        raise cv.Invalid("response_monitor entry names must be unique")
    return value


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

    # discovery: turns the hub into a passive analyzer — no framing, CRC, or TX. The framing
    # escape scheme and crc: are exactly what it is trying to discover, so they are not required
    # (and the gate/idle_command rules below do not apply). sniffer_stats: needs the validated
    # frame path that discovery bypasses, so the two cannot be combined.
    if CONF_DISCOVERY in config:
        if CONF_SNIFFER_STATS in config:
            raise cv.Invalid(
                "discovery: cannot be combined with sniffer_stats: — discovery bypasses the "
                "framing/validation path that sniffer_stats: records from"
            )
        if CONF_RESPONSE_MONITOR in config or CONF_RESPONSE_FIELDS in config:
            raise cv.Invalid(
                "discovery: cannot be combined with response_fields:/response_monitor: — "
                "discovery bypasses the framing/validation and TX path they match against"
            )
        if CONF_FRAME_TRACE in config:
            raise cv.Invalid(
                "discovery: cannot be combined with frame_trace: — discovery bypasses the "
                "framing/validation and TX path frame_trace records from"
            )
        return config

    # Non-discovery hubs must declare the framing escape scheme and a crc: block.
    if CONF_ESCAPE not in framing:
        raise cv.Invalid(
            "framing.escape is required but missing (set framing.escape.mode to escape_byte "
            f"or double); it has no default. See {DOC_FRAMING_ESCAPE_URL} for the two escape "
            "schemes. Omit it only on a discovery: hub"
        )
    if CONF_CRC not in config:
        raise cv.Invalid(
            "crc: is required (declare your device's checksum, or type: none). Omit it only "
            "on a discovery: hub"
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

    # response_monitor: signature entries (and changed_gated's gate) reference response_fields:
    # by name — cross-check every reference resolves, listing the valid names on a miss (same
    # style as light: effect: reporting an unknown effect name). button_id: trigger resolution
    # is handled separately in a FINAL_VALIDATE_SCHEMA, since it needs to walk to another
    # entity's config elsewhere in the tree.
    field_names = config.get(CONF_RESPONSE_FIELDS, {})
    for entry in config.get(CONF_RESPONSE_MONITOR, []):
        for alt in entry[CONF_SIGNATURE]:
            if alt[CONF_FIELD] not in field_names:
                raise cv.Invalid(
                    f"response_monitor entry '{entry[CONF_NAME]}': unknown field "
                    f"'{alt[CONF_FIELD]}' — valid response_fields names are "
                    f"{sorted(field_names) or '(none declared)'}"
                )
            # changed/changed_gated on a field longer than 4 bytes (e.g. a display-text
            # field) compares the field's full byte range in C++ (response_monitor.cpp's
            # eval_alt_), bypassing decode_int_/mask entirely — an explicit mask:/bit: would
            # silently do nothing there, so reject it instead of accepting dead config.
            if (
                alt[CONF_TYPE] in (SIGNATURE_TYPE_CHANGED, SIGNATURE_TYPE_CHANGED_GATED)
                and field_names[alt[CONF_FIELD]][CONF_LENGTH] > 4
                and alt[CONF_MASK] != 0xFFFFFFFF
            ):
                raise cv.Invalid(
                    f"response_monitor entry '{entry[CONF_NAME]}': mask:/bit: has no effect "
                    f"on field '{alt[CONF_FIELD]}' (length "
                    f"{field_names[alt[CONF_FIELD]][CONF_LENGTH]} > 4 bytes) — 'changed' "
                    "already compares the field's full byte range for fields this long; "
                    "remove mask:/bit: from this signature entry"
                )
            if alt[CONF_TYPE] == SIGNATURE_TYPE_CHANGED_GATED:
                gate_field = alt[CONF_GATE][CONF_FIELD]
                if gate_field not in field_names:
                    raise cv.Invalid(
                        f"response_monitor entry '{entry[CONF_NAME]}': unknown gate field "
                        f"'{gate_field}' — valid response_fields names are "
                        f"{sorted(field_names) or '(none declared)'}"
                    )

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RS485FrameHub),
            cv.Optional(CONF_FRAMING, default={}): FRAMING_SCHEMA,
            # crc.type has no sensible cross-bus default, so validate_hub() requires this block
            # for every non-discovery hub (a discovery: hub is trying to discover the CRC and
            # so does not need it). Optional here, enforced there.
            cv.Optional(CONF_CRC): CRC_SCHEMA,
            cv.Optional(CONF_DISCOVERY): DISCOVERY_SCHEMA,
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
            cv.Optional(CONF_SNIFFER_STATS): SNIFFER_STATS_SCHEMA,
            cv.Optional(CONF_FRAME_TRACE): FRAME_TRACE_SCHEMA,
            cv.Optional(CONF_RESPONSE_FIELDS): _validate_response_fields,
            cv.Optional(CONF_RESPONSE_MONITOR): _validate_response_monitor,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_hub,
)


def _resolve_button_trigger(
    button_config: dict, hub_command_format: dict | None = None
) -> list[int]:
    # Mirrors RS485FrameHub::build_key_payload_ / queue_raw_frame's on-wire byte layout so
    # button_id: resolves to the SAME bytes the button will actually transmit, keeping a
    # single source of truth instead of a hand-copied byte sequence that can drift. Only
    # covers the button forms that exist today (button/__init__.py's _validate_button);
    # anything else is a bug in that platform's own validation, not something this function
    # needs to re-guard against.
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


def _resolve_button_by_name(full_config, hub_id, button_name):
    # button_name: exists so a response_monitor entry can pin a button without that button
    # needing an explicit id: — every button already carries a name: (button.yaml-style
    # packages set it unconditionally), and it must already be unique per HA domain, so it
    # is a free, always-present join key. Scoped to buttons on this hub only, matching
    # button_id:'s own hub-ownership check above.
    return [
        button_config
        for button_config in full_config.get(CONF_BUTTON, [])
        if button_config.get(CONF_RS485_FRAME_ID) == hub_id
        and button_config.get(CONF_NAME) == button_name
    ]


def _final_validate_response_monitor(config):
    if CONF_RESPONSE_MONITOR not in config:
        return config
    full_config = fv.full_config.get()
    hub_id = config[CONF_ID]
    for entry in config[CONF_RESPONSE_MONITOR]:
        trigger = entry[CONF_TRIGGER]
        button_id = trigger.get(CONF_BUTTON_ID)
        button_name = trigger.get(CONF_BUTTON_NAME)
        if button_id is None and button_name is None:
            continue
        if button_id is not None:
            button_path = full_config.get_path_for_id(button_id)[:-1]
            # button_path[0] is the top-level YAML domain key the id was declared under (e.g.
            # "button", "sensor", "number") — button_id: must point at an actual button: entry,
            # not merely any entity that happens to share this hub's rs485_frame_id (a sensor,
            # number, or text_sensor config has no CONF_FRAME_TYPE/CONF_PAYLOAD/CONF_VALUE for
            # _resolve_button_trigger to read, which would otherwise raise an unhandled KeyError
            # instead of this clean cv.Invalid).
            button_config = full_config.get_config_for_path(button_path)
            if (
                button_path[0] != CONF_BUTTON
                or button_config.get(CONF_RS485_FRAME_ID) != hub_id
            ):
                raise cv.Invalid(
                    f"response_monitor entry '{entry[CONF_NAME]}': button_id "
                    f"'{button_id}' is not a button on this hub"
                )
            ref = f"button_id '{button_id}'"
        else:
            matches = _resolve_button_by_name(full_config, hub_id, button_name)
            if len(matches) != 1:
                raise cv.Invalid(
                    f"response_monitor entry '{entry[CONF_NAME]}': button_name "
                    f"'{button_name}' matches {len(matches)} buttons on this hub "
                    "(must match exactly one)"
                )
            button_config = matches[0]
            ref = f"button_name '{button_name}'"

        hub_command_format = None
        if CONF_VALUE in button_config and CONF_COMMAND_FORMAT not in button_config:
            if CONF_COMMAND_FORMAT not in config:
                # Mirrors button/__init__.py's own _final_validate error for this exact
                # condition — reached first here because this hub-level validator runs
                # before the button platform's own final_validate does when rs485_frame: is
                # declared before button: in YAML (the typical order).
                raise cv.Invalid(
                    f"response_monitor entry '{entry[CONF_NAME]}': {ref} "
                    "uses 'value:' but neither the button nor the referenced hub has a "
                    "'command_format:' block, so the value has no defined on-wire encoding"
                )
            # Passed explicitly (not stashed on button_config) so this hub-level validator
            # doesn't mutate the shared config dict that button/__init__.py's own to_code()/
            # _final_validate() subsequently reads.
            hub_command_format = config[CONF_COMMAND_FORMAT]
        entry["_resolved_trigger"] = _resolve_button_trigger(
            button_config, hub_command_format
        )
        if len(entry["_resolved_trigger"]) > MAX_RESPONSE_TRIGGER_LEN:
            raise cv.Invalid(
                f"response_monitor entry '{entry[CONF_NAME]}': {ref} "
                f"resolves to a {len(entry['_resolved_trigger'])}-byte trigger, exceeding the "
                f"{MAX_RESPONSE_TRIGGER_LEN}-byte cap"
            )
    return config


_DUMP_FRAME_TRACE_ACTION_KEY = "rs485_frame.dump_frame_trace"


def _iter_action_configs(node, action_key):
    """Recursively find every validated config value for `action_key` anywhere in the full
    config tree. Actions are registered globally (not scoped to a component's own domain), so
    rs485_frame.dump_frame_trace can appear nested inside any other component's automation
    blocks -- e.g. a switch's on_turn_on:, not just this hub's own response_monitor entries.
    A generic tree walk is the only way to find every occurrence.
    """
    if isinstance(node, dict):
        for key, value in node.items():
            if key == action_key:
                yield value
            else:
                yield from _iter_action_configs(value, action_key)
    elif isinstance(node, list):
        for item in node:
            yield from _iter_action_configs(item, action_key)


def _final_validate_frame_trace_actions(config):
    if CONF_FRAME_TRACE in config:
        return config
    hub_id = config[CONF_ID]
    full_config = fv.full_config.get()
    for action_config in _iter_action_configs(
        full_config, _DUMP_FRAME_TRACE_ACTION_KEY
    ):
        target_id = (
            action_config[CONF_ID] if isinstance(action_config, dict) else action_config
        )
        if target_id == hub_id:
            raise cv.Invalid(
                f"rs485_frame.dump_frame_trace references hub '{hub_id}', which has no "
                "frame_trace: block configured"
            )
    return config


def _final_validate(config):
    config = _final_validate_response_monitor(config)
    return _final_validate_frame_trace_actions(config)


FINAL_VALIDATE_SCHEMA = _final_validate


# response_monitor:'s on_confirmed:/on_failed: are per-entry (registered via
# add_response_monitor_on_confirmed_callback(entry_index, ...)), so this can't reuse
# automation.build_callback_automation as-is -- that helper's callback_method call takes only
# the forwarder, with no room for the entry_index the C++ side needs to route the callback to
# the right ResponseMonitorEntry. Same body otherwise (see automation.build_callback_automation).
async def _build_response_monitor_callback(
    var: cg.MockObj, callback_method: str, entry_index: int, conf: ConfigType
) -> None:
    templ = cg.TemplateArguments()
    obj = cg.new_Pvariable(conf[CONF_AUTOMATION_ID], templ)
    actions = await automation.build_action_list(conf[CONF_THEN], templ, [])
    cg.add(obj.add_actions(actions))
    forwarder = automation.TriggerForwarder.template(templ)
    cg.add(
        getattr(var, callback_method)(
            entry_index, cg.RawExpression(f"{forwarder}{{{obj}}}")
        )
    )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    framing = config[CONF_FRAMING]
    # escape is absent only on a discovery: hub (validate_hub requires it otherwise). When
    # present, the runtime tracks a single escape marker byte: in double mode a literal DLE is
    # stuffed as DLE DLE, so the marker is the DLE byte itself; in escape_byte mode it is the
    # declared byte. Resolving it here keeps the C++ framer/encoder a single unified code path.
    if (escape := framing.get(CONF_ESCAPE)) is not None:
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

    # crc is absent only on a discovery: hub.
    if (crc := config.get(CONF_CRC)) is not None:
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

    if (stats := config.get(CONF_SNIFFER_STATS)) is not None:
        # cg.add_define gates the SnifferStats field, includes, and hot-path call out of
        # builds that don't use sniffer_stats — production firmware pays no cost at all.
        cg.add_define("USE_RS485_FRAME_SNIFFER_STATS")
        # reference_frame_type defaults to the gate frame_type (may be empty for non
        # frame_trigger modes); the sniffer treats an empty reference as "no cadence ref".
        ref = stats.get(CONF_REFERENCE_FRAME_TYPE, gate_frame_type)
        cg.add(
            var.enable_sniffer_stats(
                stats[CONF_MAX_FRAME_TYPES],
                stats[CONF_INTERVAL].total_milliseconds,
                stats[CONF_PAYLOAD_DUMP_TOP],
                stats[CONF_MAX_UNIQUE_PAYLOADS],
                stats[CONF_PAYLOAD_CAPTURE_BYTES],
                ref,
                stats[CONF_ASCII_STRIP_HIGH_BIT],
                stats[CONF_REFERENCE_MODE] == "send",
            )
        )

    if (trace := config.get(CONF_FRAME_TRACE)) is not None:
        # Gates the FrameTrace field, includes, and the RX/TX/reject-path hooks out of builds
        # that don't use frame_trace: — production firmware pays no cost at all.
        cg.add_define("USE_RS485_FRAME_FRAME_TRACE")
        cg.add(
            var.enable_frame_trace(
                trace[CONF_SIZE],
                trace[CONF_CAPTURE_BYTES],
                trace[CONF_INCLUDE_INVALID],
            )
        )

    if (disc := config.get(CONF_DISCOVERY)) is not None:
        # Gates the discovery field, includes, and the loop() bypass out of builds that don't
        # use it. The burst buffer is capped at max_frame_length so long frames still fit.
        cg.add_define("USE_RS485_FRAME_DISCOVERY")
        cg.add(
            var.enable_discovery(
                disc[CONF_INTERVAL].total_milliseconds,
                disc[CONF_IDLE_GAP].total_milliseconds,
                config[CONF_MAX_FRAME_LENGTH],
                disc[CONF_MIN_FRAMING_CONFIDENCE],
            )
        )
        # baud_sweep is optional: only wire the sweep when the user asked discovery to find the
        # baud rate. data_bits_sweep and dwell have defaults, so they are always present here.
        if (bauds := disc.get(CONF_BAUD_SWEEP)) is not None:
            cg.add(
                var.configure_discovery_baud_sweep(
                    bauds,
                    disc[CONF_DATA_BITS_SWEEP],
                    disc[CONF_DWELL].total_milliseconds,
                )
            )

    if CONF_RESPONSE_FIELDS in config or CONF_RESPONSE_MONITOR in config:
        # Gates the ResponseMonitor field, includes, and the TX/RX hooks out of builds that
        # don't use it. Production builds (without the define) pay no cost at all.
        cg.add_define("USE_RS485_FRAME_RESPONSE_MONITOR")
        cg.add(var.enable_response_monitor())

        # response_fields: is an ordered YAML mapping (Python dicts preserve insertion order);
        # that order IS each field's runtime index — field_index below is a plain position
        # lookup, no ID registry involved.
        field_names = list(config.get(CONF_RESPONSE_FIELDS, {}))
        for name in field_names:
            field = config[CONF_RESPONSE_FIELDS][name]
            frame_type = list(field[CONF_FRAME_TYPE])
            # frame_type_mask defaults to all-0xFF (today's exact-match behavior) when
            # omitted — resolved here so the C++ side never needs to special-case "no mask".
            mask = list(field.get(CONF_FRAME_TYPE_MASK, [0xFF] * len(frame_type)))
            cg.add(
                var.add_response_field(
                    frame_type,
                    mask,
                    field[CONF_OFFSET],
                    field[CONF_LENGTH],
                    field[CONF_ENDIAN] == "big",
                )
            )

        for entry_index, entry in enumerate(config.get(CONF_RESPONSE_MONITOR, [])):
            # button_id: resolution happened in FINAL_VALIDATE_SCHEMA (needs a full_config
            # walk to another entity); the literal frame_type: form needs no resolution.
            trigger_bytes = entry.get("_resolved_trigger") or list(
                entry[CONF_TRIGGER].get(CONF_FRAME_TYPE, [])
            )
            # add_response_monitor_entry's runtime return value is this same entry_index —
            # entries are added in this exact order, so the Python-side enumerate() index and
            # the C++-side StaticVector position always agree; no need for the call's return.
            cg.add(
                var.add_response_monitor_entry(
                    trigger_bytes, entry[CONF_WINDOW].total_milliseconds
                )
            )
            for alt in entry[CONF_SIGNATURE]:
                field_index = field_names.index(alt[CONF_FIELD])
                if alt[CONF_TYPE] == SIGNATURE_TYPE_MASKED_INT:
                    cg.add(
                        var.add_response_monitor_masked_int_alt(
                            entry_index,
                            field_index,
                            alt[CONF_MASK],
                            list(alt[CONF_VALUE]),
                        )
                    )
                elif alt[CONF_TYPE] == SIGNATURE_TYPE_TEXT_ENUM:
                    cg.add(
                        var.add_response_monitor_text_enum_alt(
                            entry_index, field_index, list(alt[CONF_VALUES])
                        )
                    )
                elif alt[CONF_TYPE] == SIGNATURE_TYPE_CHANGED:
                    cg.add(
                        var.add_response_monitor_changed_alt(
                            entry_index, field_index, alt[CONF_MASK]
                        )
                    )
                else:  # changed_gated
                    gate = alt[CONF_GATE]
                    gate_field_index = field_names.index(gate[CONF_FIELD])
                    cg.add(
                        var.add_response_monitor_changed_gated_alt(
                            entry_index,
                            field_index,
                            alt[CONF_MASK],
                            gate_field_index,
                            gate[CONF_MASK],
                            gate[CONF_VALUE],
                        )
                    )

            for conf in entry.get(CONF_ON_CONFIRMED, []):
                await _build_response_monitor_callback(
                    var, "add_response_monitor_on_confirmed_callback", entry_index, conf
                )
            for conf in entry.get(CONF_ON_FAILED, []):
                await _build_response_monitor_callback(
                    var, "add_response_monitor_on_failed_callback", entry_index, conf
                )

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
# emit anything (probe frames, vendor-specific commands, device-discovery sequences, or a
# value computed at runtime) that the `command:` button shorthand cannot express.
SEND_FRAME_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RS485FrameHub),
        cv.Required(CONF_FRAME_TYPE): cv.templatable(validate_frame_type),
        cv.Required(CONF_PAYLOAD): cv.templatable(cv.ensure_list(validate_byte)),
    }
)


@automation.register_action(
    "rs485_frame.send_frame", SendFrameAction, SEND_FRAME_SCHEMA, synchronous=True
)
async def send_frame_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    frame_type = await cg.templatable(
        config[CONF_FRAME_TYPE], args, cg.std_vector.template(cg.uint8)
    )
    cg.add(var.set_frame_type(frame_type))
    payload = await cg.templatable(
        config[CONF_PAYLOAD], args, cg.std_vector.template(cg.uint8)
    )
    cg.add(var.set_payload(payload))
    return var


# rs485_frame.dump_frame_trace: log the hub's frame_trace ring buffer (recent RX/TX frames,
# tagged direction and validity) to the console. Typically wired into a response_monitor
# entry's on_failed: to capture what was actually on the wire around a confirmation failure,
# but usable from anywhere (e.g. on_boot:) that has this hub's id. Rejected at validation time
# (see _final_validate_frame_trace_actions above) if the target hub has no frame_trace: block.
DUMP_FRAME_TRACE_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(RS485FrameHub),
    }
)


@automation.register_action(
    "rs485_frame.dump_frame_trace",
    DumpFrameTraceAction,
    DUMP_FRAME_TRACE_SCHEMA,
    synchronous=True,
)
async def dump_frame_trace_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
