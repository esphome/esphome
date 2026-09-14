from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_client
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_DATA,
    CONF_ID,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_ON_MESSAGE,
    CONF_PAYLOAD,
    CONF_TRIGGER_ID,
    CONF_VALUE,
)

CODEOWNERS = ["@bogdanr"]
DEPENDENCIES = ["ble_client"]
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
CONF_VELOCITY = "velocity"

ble_midi_ns = cg.esphome_ns.namespace("ble_midi")
BLEMidi = ble_midi_ns.class_("BLEMidi", cg.Component, ble_client.BLEClientNode)

ConnectTrigger = ble_midi_ns.class_("ConnectTrigger", automation.Trigger.template())
DisconnectTrigger = ble_midi_ns.class_(
    "DisconnectTrigger", automation.Trigger.template()
)
NoteOnTrigger = ble_midi_ns.class_(
    "NoteOnTrigger", automation.Trigger.template(cg.uint8, cg.uint8, cg.uint8)
)
NoteOffTrigger = ble_midi_ns.class_(
    "NoteOffTrigger", automation.Trigger.template(cg.uint8, cg.uint8, cg.uint8)
)
ControlChangeTrigger = ble_midi_ns.class_(
    "ControlChangeTrigger", automation.Trigger.template(cg.uint8, cg.uint8, cg.uint8)
)
ProgramChangeTrigger = ble_midi_ns.class_(
    "ProgramChangeTrigger", automation.Trigger.template(cg.uint8, cg.uint8)
)
PitchBendTrigger = ble_midi_ns.class_(
    "PitchBendTrigger", automation.Trigger.template(cg.int16, cg.uint8)
)
SysexTrigger = ble_midi_ns.class_(
    "SysexTrigger", automation.Trigger.template(cg.std_vector.template(cg.uint8))
)
MessageTrigger = ble_midi_ns.class_(
    "MessageTrigger", automation.Trigger.template(cg.std_vector.template(cg.uint8))
)

NoteOnAction = ble_midi_ns.class_("NoteOnAction", automation.Action)
NoteOffAction = ble_midi_ns.class_("NoteOffAction", automation.Action)
ControlChangeAction = ble_midi_ns.class_("ControlChangeAction", automation.Action)
ProgramChangeAction = ble_midi_ns.class_("ProgramChangeAction", automation.Action)
PitchBendAction = ble_midi_ns.class_("PitchBendAction", automation.Action)
SysexAction = ble_midi_ns.class_("SysexAction", automation.Action)
RawAction = ble_midi_ns.class_("RawAction", automation.Action)
BLEMidiConnectedCondition = ble_midi_ns.class_(
    "BLEMidiConnectedCondition", automation.Condition
)

# MIDI data bytes are 7-bit and channels are 4-bit, matching what goes on the wire and what the triggers report.
# Channel 0 is the channel that MIDI hardware usually labels "1".
midi_data_byte = cv.int_range(min=0, max=127)
midi_channel = cv.int_range(min=0, max=15)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLEMidi),
            cv.Optional(CONF_MAX_SYSEX_SIZE, default=256): cv.int_range(
                min=8, max=2048
            ),
            cv.Optional(CONF_PAIR, default=False): cv.boolean,
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ConnectTrigger)}
            ),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DisconnectTrigger)}
            ),
            cv.Optional(CONF_ON_NOTE_ON): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NoteOnTrigger)}
            ),
            cv.Optional(CONF_ON_NOTE_OFF): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NoteOffTrigger)}
            ),
            cv.Optional(CONF_ON_CONTROL_CHANGE): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ControlChangeTrigger)}
            ),
            cv.Optional(CONF_ON_PROGRAM_CHANGE): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ProgramChangeTrigger)}
            ),
            cv.Optional(CONF_ON_PITCH_BEND): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PitchBendTrigger)}
            ),
            cv.Optional(CONF_ON_SYSEX): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SysexTrigger)}
            ),
            cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MessageTrigger)}
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    cg.add(var.set_max_sysex_size(config[CONF_MAX_SYSEX_SIZE]))
    cg.add(var.set_pair(config[CONF_PAIR]))

    note_args = [
        (cg.uint8, CONF_NOTE),
        (cg.uint8, CONF_VELOCITY),
        (cg.uint8, CONF_CHANNEL),
    ]
    bytes_args = [(cg.std_vector.template(cg.uint8), CONF_DATA)]
    triggers = (
        (CONF_ON_CONNECT, []),
        (CONF_ON_DISCONNECT, []),
        (CONF_ON_NOTE_ON, note_args),
        (CONF_ON_NOTE_OFF, note_args),
        (
            CONF_ON_CONTROL_CHANGE,
            [
                (cg.uint8, CONF_CONTROL_NUMBER),
                (cg.uint8, CONF_VALUE),
                (cg.uint8, CONF_CHANNEL),
            ],
        ),
        (CONF_ON_PROGRAM_CHANGE, [(cg.uint8, CONF_PROGRAM), (cg.uint8, CONF_CHANNEL)]),
        (CONF_ON_PITCH_BEND, [(cg.int16, CONF_VALUE), (cg.uint8, CONF_CHANNEL)]),
        (CONF_ON_SYSEX, bytes_args),
        (CONF_ON_MESSAGE, bytes_args),
    )
    for key, args in triggers:
        for conf in config.get(key, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, args, conf)


def _action_schema(keys: dict) -> cv.Schema:
    return cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BLEMidi),
            **keys,
            cv.Optional(CONF_CHANNEL, default=0): cv.templatable(midi_channel),
        }
    )


async def _build_action(config, action_id, template_arg, args, keys):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, parent)
    for key, type_ in {**keys, CONF_CHANNEL: cg.uint8}.items():
        template_ = await cg.templatable(config[key], args, type_)
        cg.add(getattr(var, f"set_{key}")(template_))
    return var


NOTE_ACTION_SCHEMA = _action_schema(
    {
        cv.Required(CONF_NOTE): cv.templatable(midi_data_byte),
        cv.Optional(CONF_VELOCITY, default=64): cv.templatable(midi_data_byte),
    }
)


@automation.register_action(
    "ble_midi.send_note_on", NoteOnAction, NOTE_ACTION_SCHEMA, synchronous=True
)
async def send_note_on_to_code(config, action_id, template_arg, args):
    return await _build_action(
        config,
        action_id,
        template_arg,
        args,
        {CONF_NOTE: cg.uint8, CONF_VELOCITY: cg.uint8},
    )


@automation.register_action(
    "ble_midi.send_note_off", NoteOffAction, NOTE_ACTION_SCHEMA, synchronous=True
)
async def send_note_off_to_code(config, action_id, template_arg, args):
    return await _build_action(
        config,
        action_id,
        template_arg,
        args,
        {CONF_NOTE: cg.uint8, CONF_VELOCITY: cg.uint8},
    )


@automation.register_action(
    "ble_midi.send_control_change",
    ControlChangeAction,
    _action_schema(
        {
            cv.Required(CONF_CONTROL_NUMBER): cv.templatable(midi_data_byte),
            cv.Required(CONF_VALUE): cv.templatable(midi_data_byte),
        }
    ),
    synchronous=True,
)
async def send_control_change_to_code(config, action_id, template_arg, args):
    return await _build_action(
        config,
        action_id,
        template_arg,
        args,
        {CONF_CONTROL_NUMBER: cg.uint8, CONF_VALUE: cg.uint8},
    )


@automation.register_action(
    "ble_midi.send_program_change",
    ProgramChangeAction,
    _action_schema({cv.Required(CONF_PROGRAM): cv.templatable(midi_data_byte)}),
    synchronous=True,
)
async def send_program_change_to_code(config, action_id, template_arg, args):
    return await _build_action(
        config, action_id, template_arg, args, {CONF_PROGRAM: cg.uint8}
    )


@automation.register_action(
    "ble_midi.send_pitch_bend",
    PitchBendAction,
    _action_schema(
        {
            cv.Required(CONF_VALUE): cv.templatable(cv.int_range(min=-8192, max=8191)),
        }
    ),
    synchronous=True,
)
async def send_pitch_bend_to_code(config, action_id, template_arg, args):
    return await _build_action(
        config, action_id, template_arg, args, {CONF_VALUE: cg.int16}
    )


@automation.register_action(
    "ble_midi.send_sysex",
    SysexAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BLEMidi),
            cv.Required(CONF_PAYLOAD): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
        }
    ),
    synchronous=True,
)
async def send_sysex_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, parent)
    payload = await cg.templatable(
        config[CONF_PAYLOAD], args, cg.std_vector.template(cg.uint8)
    )
    cg.add(var.set_payload(payload))
    return var


@automation.register_action(
    "ble_midi.send_raw",
    RawAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BLEMidi),
            cv.Required(CONF_DATA): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
        }
    ),
    synchronous=True,
)
async def send_raw_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, parent)
    data = await cg.templatable(
        config[CONF_DATA], args, cg.std_vector.template(cg.uint8)
    )
    cg.add(var.set_data(data))
    return var


@automation.register_condition(
    "ble_midi.connected",
    BLEMidiConnectedCondition,
    cv.Schema({cv.GenerateID(): cv.use_id(BLEMidi)}),
)
async def connected_to_code(config, condition_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, parent)
    return var
