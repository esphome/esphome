from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_client
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_DATA,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_ON_MESSAGE,
    CONF_PAYLOAD,
    CONF_VALUE,
)
from esphome.types import ConfigType

CODEOWNERS = ["@bogdanr"]
DEPENDENCIES = ["ble_client"]
DOMAIN = "ble_midi"
MULTI_CONF = True

CONF_CONTROL_NUMBER = "control_number"
CONF_MAX_SYSEX_SIZE = "max_sysex_size"
CONF_NOTE = "note"
CONF_ON_CONTROL_CHANGE = "on_control_change"
CONF_ON_NOTE_OFF = "on_note_off"
CONF_ON_NOTE_ON = "on_note_on"
CONF_ON_PITCH_BEND = "on_pitch_bend"
CONF_ON_PROGRAM_CHANGE = "on_program_change"
CONF_ON_SYSEX = "on_sysex"
CONF_PAIR = "pair"
CONF_PROGRAM = "program"
CONF_SYSEX_BUFFER_ID = "sysex_buffer_id"
CONF_VELOCITY = "velocity"

ble_midi_ns = cg.esphome_ns.namespace("ble_midi")
BLEMidi = ble_midi_ns.class_("BLEMidi", cg.Component, ble_client.BLEClientNode)
# The message bytes as a view: data.size() and data[i] work, and nothing is copied
ByteSpan = cg.std_span.template(cg.uint8.operator("const"))

# MIDI data bytes are 7-bit and channels are 4-bit, matching what goes on the wire and what the triggers report.
# Channel 0 is the channel that MIDI hardware usually labels "1".
midi_data_byte = cv.int_range(min=0, max=127)
midi_channel = cv.int_range(min=0, max=15)

_NOTE_ARGS = [
    (cg.uint8, CONF_NOTE),
    (cg.uint8, CONF_VELOCITY),
    (cg.uint8, CONF_CHANNEL),
]
_BYTES_ARGS = [(ByteSpan, CONF_DATA)]
_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_CONNECT, "add_on_connect_callback"),
    automation.CallbackAutomation(CONF_ON_DISCONNECT, "add_on_disconnect_callback"),
    automation.CallbackAutomation(
        CONF_ON_NOTE_ON, "add_on_note_on_callback", _NOTE_ARGS
    ),
    automation.CallbackAutomation(
        CONF_ON_NOTE_OFF, "add_on_note_off_callback", _NOTE_ARGS
    ),
    automation.CallbackAutomation(
        CONF_ON_CONTROL_CHANGE,
        "add_on_control_change_callback",
        [
            (cg.uint8, CONF_CONTROL_NUMBER),
            (cg.uint8, CONF_VALUE),
            (cg.uint8, CONF_CHANNEL),
        ],
    ),
    automation.CallbackAutomation(
        CONF_ON_PROGRAM_CHANGE,
        "add_on_program_change_callback",
        [(cg.uint8, CONF_PROGRAM), (cg.uint8, CONF_CHANNEL)],
    ),
    automation.CallbackAutomation(
        CONF_ON_PITCH_BEND,
        "add_on_pitch_bend_callback",
        [(cg.int16, CONF_VALUE), (cg.uint8, CONF_CHANNEL)],
    ),
    automation.CallbackAutomation(CONF_ON_SYSEX, "add_on_sysex_callback", _BYTES_ARGS),
    automation.CallbackAutomation(
        CONF_ON_MESSAGE, "add_on_message_callback", _BYTES_ARGS
    ),
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLEMidi),
            cv.GenerateID(CONF_SYSEX_BUFFER_ID): cv.declare_id(cg.uint8),
            cv.Optional(CONF_MAX_SYSEX_SIZE, default=256): cv.int_range(
                min=8, max=2048
            ),
            cv.Optional(CONF_PAIR, default=False): cv.boolean,
            **{
                cv.Optional(entry.conf_key): automation.validate_automation({})
                for entry in _CALLBACK_AUTOMATIONS
            },
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    size = config[CONF_MAX_SYSEX_SIZE]
    # SysEx messages are reassembled in a static array, so nothing is allocated at run time
    buf = config[CONF_SYSEX_BUFFER_ID].id
    cg.add_global(cg.RawStatement(f"static uint8_t {buf}[{size}];"))
    var = await ble_client.new_ble_node_component(
        config, cg.RawExpression(buf), size, config[CONF_PAIR]
    )
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


def _action_schema(keys: dict) -> cv.Schema:
    return cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BLEMidi),
            **keys,
            cv.Optional(CONF_CHANNEL, default=0): cv.templatable(midi_channel),
        }
    )


_NOTE_ACTION_SCHEMA = _action_schema(
    {
        cv.Required(CONF_NOTE): cv.templatable(midi_data_byte),
        cv.Optional(CONF_VELOCITY, default=64): cv.templatable(midi_data_byte),
    }
)

_CHANNEL = (CONF_CHANNEL, cg.uint8)
_NOTE_CALL_ARGS = ((CONF_NOTE, cg.uint8), (CONF_VELOCITY, cg.uint8), _CHANNEL)

automation.register_apply_action(
    "ble_midi.send_note_on",
    _NOTE_ACTION_SCHEMA,
    automation.ApplyCall("send_note_on({}, {}, {})", _NOTE_CALL_ARGS),
)

automation.register_apply_action(
    "ble_midi.send_note_off",
    _NOTE_ACTION_SCHEMA,
    automation.ApplyCall("send_note_off({}, {}, {})", _NOTE_CALL_ARGS),
)

automation.register_apply_action(
    "ble_midi.send_control_change",
    _action_schema(
        {
            cv.Required(CONF_CONTROL_NUMBER): cv.templatable(midi_data_byte),
            cv.Required(CONF_VALUE): cv.templatable(midi_data_byte),
        }
    ),
    automation.ApplyCall(
        "send_control_change({}, {}, {})",
        ((CONF_CONTROL_NUMBER, cg.uint8), (CONF_VALUE, cg.uint8), _CHANNEL),
    ),
)

automation.register_apply_action(
    "ble_midi.send_program_change",
    _action_schema({cv.Required(CONF_PROGRAM): cv.templatable(midi_data_byte)}),
    automation.ApplyCall(
        "send_program_change({}, {})", ((CONF_PROGRAM, cg.uint8), _CHANNEL)
    ),
)

automation.register_apply_action(
    "ble_midi.send_pitch_bend",
    _action_schema(
        {
            cv.Required(CONF_VALUE): cv.templatable(cv.int_range(min=-8192, max=8191)),
        }
    ),
    automation.ApplyCall("send_pitch_bend({}, {})", ((CONF_VALUE, cg.int16), _CHANNEL)),
)


def _bytes_action_schema(key: str) -> cv.Schema:
    return cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BLEMidi),
            cv.Required(key): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
        }
    )


automation.register_apply_action(
    "ble_midi.send_sysex",
    _bytes_action_schema(CONF_PAYLOAD),
    automation.ApplyBytes(CONF_PAYLOAD, "send_sysex({})"),
)

automation.register_apply_action(
    "ble_midi.send_raw",
    _bytes_action_schema(CONF_DATA),
    automation.ApplyBytes(CONF_DATA, "send_raw({})"),
)

automation.register_apply_condition(
    "ble_midi.connected", cv.Schema({cv.GenerateID(): cv.use_id(BLEMidi)}), "is_ready()"
)
