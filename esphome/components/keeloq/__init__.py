from collections.abc import Iterator

from esphome import automation, preferences
import esphome.codegen as cg
from esphome.components import remote_base
from esphome.components.const import CONF_ON_FRAME
from esphome.components.remote_base import CONF_RECEIVER_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_CHANNELS,
    CONF_COMMAND,
    CONF_ID,
    CONF_REPEAT,
    CONF_TIMES,
    CONF_TYPE,
    CONF_WAIT_TIME,
)
from esphome.core import ID, HexInt
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.helpers import fnv1a_32bit_hash
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@Bascht74"]
AUTO_LOAD = ["remote_base"]
DOMAIN = "keeloq"
MULTI_CONF = True

CONF_COUNTER = "counter"
CONF_MANUFACTURER_KEY = "manufacturer_key"
CONF_ON_COMMAND = "on_command"
CONF_SERIAL = "serial"

keeloq_ns = cg.esphome_ns.namespace("keeloq")
KeeloqRemote = keeloq_ns.class_("KeeloqRemote", cg.Component)
KeeloqFrame = keeloq_ns.struct("KeeloqFrame")
_FRAME_ARGS = [(KeeloqFrame, "x")]
ANY_CHANNEL = 0
ANY_COMMAND = 0xFF

KeeloqType = keeloq_ns.enum("KeeloqType")
KEELOQ_TYPES = KeeloqType.options("jarolift", prefix="KEELOQ_TYPE_")

# Jarolift sends every press twice with the same counter.
JAROLIFT_SEND_TIMES = 2
# Button name: (code, send times). Shade is stop held, sent 20 times.
JAROLIFT_COMMANDS = {
    "up": (0x8, JAROLIFT_SEND_TIMES),
    "down": (0x2, JAROLIFT_SEND_TIMES),
    "stop": (0x4, JAROLIFT_SEND_TIMES),
    "learn": (0xA, JAROLIFT_SEND_TIMES),
    "shade": (0x4, 20),
}

_NLF = 0x3A5C742E


def _bit(value: int, n: int) -> int:
    return (value >> n) & 1


def keeloq_decrypt(data: int, key: int) -> int:
    """The KeeLoq block cipher decryption: 32-bit block, 64-bit key, 528 rounds."""
    x = data
    for r in range(528):
        nlf = _bit(
            _NLF,
            _bit(x, 0)
            | _bit(x, 8) << 1
            | _bit(x, 19) << 2
            | _bit(x, 25) << 3
            | _bit(x, 30) << 4,
        )
        x = ((x << 1) & 0xFFFFFFFF) | (
            _bit(x, 31) ^ _bit(x, 15) ^ nlf ^ _bit(key, (15 - r) & 63)
        )
    return x


def normal_learning_key(serial: int, manufacturer_key: int) -> int:
    """Microchip normal learning: the serial decrypted twice with the manufacturer key."""
    high = keeloq_decrypt(serial | 0x60000000, manufacturer_key)
    return high << 32 | keeloq_decrypt(serial | 0x20000000, manufacturer_key)


_COMMAND_ERROR = f"command must be 0 to 0xF or one of {', '.join(JAROLIFT_COMMANDS)}"
_command = cv.Any(
    cv.one_of(*JAROLIFT_COMMANDS, lower=True),
    cv.int_range(min=0, max=0xF),
    msg=_COMMAND_ERROR,
)


def _command_code(command: int | str) -> int:
    return JAROLIFT_COMMANDS[command][0] if isinstance(command, str) else command


def _channels(value: object) -> int:
    """Channels 1 to 16 as a list, or the 16-bit mask they make (bit 0 is channel 1)."""
    if isinstance(value, list):
        channels = {cv.int_range(min=1, max=16)(channel) for channel in value}
        value = sum(1 << (channel - 1) for channel in channels)
    mask = cv.hex_uint16_t(value)
    if mask == 0:
        raise cv.Invalid("set at least one channel")
    return mask


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(KeeloqRemote),
        cv.Required(CONF_TYPE): cv.enum(KEELOQ_TYPES, lower=True),
        cv.Required(CONF_MANUFACTURER_KEY): cv.sensitive(cv.hex_uint64_t),
        # Without a serial the remote takes serial and counter over from a hand remote.
        cv.Optional(CONF_SERIAL): cv.hex_int_range(min=0, max=0xFFFFFFF),
        cv.Optional(CONF_CHANNELS): _channels,
        cv.Optional(CONF_RECEIVER_ID): cv.use_id(remote_base.RemoteReceiverBase),
        cv.Optional(CONF_ON_FRAME): automation.validate_automation({}),
        # Like a motor: a new press of this serial, once, with replay protection.
        cv.Optional(CONF_ON_COMMAND): automation.validate_automation(
            {
                cv.Optional(CONF_CHANNEL): cv.int_range(min=1, max=16),
                cv.Optional(CONF_COMMAND): _command,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _frames_need_a_receiver(config: ConfigType) -> ConfigType:
    for key in (CONF_ON_FRAME, CONF_ON_COMMAND):
        if key in config and CONF_RECEIVER_ID not in config:
            raise cv.Invalid(f"{key} needs receiver_id", [key])
    if CONF_SERIAL not in config and CONF_RECEIVER_ID not in config:
        raise cv.Invalid(
            "Set serial, or receiver_id to take the serial over from a hand remote",
            [CONF_SERIAL],
        )
    return config


def _take_over_needs_an_id(config: ConfigType) -> ConfigType:
    # A generated id passes to a remote added before it, and a second remote makes every keeloq action name an id.
    if CONF_SERIAL not in config and not config[CONF_ID].is_manual:
        raise cv.Invalid(
            "Set id on a remote without serial; what it takes over is stored under its id",
            [CONF_ID],
        )
    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _frames_need_a_receiver, _take_over_needs_an_id)


def _final_validate(config: ConfigType) -> ConfigType:
    # The counter is stored per type and serial, so two remotes with both equal would reuse codes. A remote without
    # serial stores what it takes over under its id.
    if CONF_SERIAL not in config:
        return config
    key = (config[CONF_TYPE], config[CONF_SERIAL])
    remotes = fv.full_config.get()[DOMAIN]
    if sum((r[CONF_TYPE], r.get(CONF_SERIAL)) == key for r in remotes) > 1:
        raise cv.Invalid(
            f"serial 0x{config[CONF_SERIAL]:07X} is used by more than one remote",
            [CONF_SERIAL],
        )
    return config


def _find_actions(
    node: object, path: list, keys: tuple[str, ...]
) -> list[tuple[list, ConfigType]]:
    """Every action named in keys in the configuration, with its path."""
    found = []
    if isinstance(node, dict):
        for key, value in node.items():
            if key in keys and isinstance(value, dict):
                found.append((path + [key], value))
            found += _find_actions(value, path + [key], keys)
    elif isinstance(node, list):
        for index, value in enumerate(node):
            found += _find_actions(value, path + [index], keys)
    return found


def _actions_of(
    config: ConfigType, names: tuple[str, ...]
) -> Iterator[tuple[list, ConfigType]]:
    """Every action named in names on this remote, with the path to report it at."""
    for path, action in _find_actions(fv.full_config.get(), [], names):
        if str(action[CONF_ID]) == str(config[CONF_ID]):
            yield [cv.ROOT_CONFIG_PATH] + path, action


def _take_over_has_a_receiver(config: ConfigType) -> ConfigType:
    # keeloq.take_over waits for a frame of the hand remote.
    if CONF_RECEIVER_ID in config:
        return config
    for path, _ in _actions_of(config, ("keeloq.take_over",)):
        with cv.prepend_path(path):
            raise cv.Invalid(f"keeloq.take_over needs receiver_id on {config[CONF_ID]}")
    return config


def _learn_has_a_serial(config: ConfigType) -> ConfigType:
    # keeloq.learn pairs the remote's own serial.
    if CONF_SERIAL in config:
        return config
    for path, _ in _actions_of(config, ("keeloq.learn",)):
        with cv.prepend_path(path):
            raise cv.Invalid(
                f"keeloq.learn needs serial on {config[CONF_ID]}; a remote without serial takes over a hand remote"
            )
    return config


def _actions_have_channels(config: ConfigType) -> ConfigType:
    # A remote with serial has no hand remote to take channels from.
    if CONF_CHANNELS in config or CONF_SERIAL not in config:
        return config
    sending = ("keeloq.send", "keeloq.learn")
    for path, action in _actions_of(config, sending):
        if CONF_CHANNELS in action:
            continue
        with cv.prepend_path(path):
            raise cv.Invalid(
                f"{action[CONF_ID]} has no channels; set channels on the action or the remote",
                [CONF_CHANNELS],
            )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    _final_validate,
    _take_over_has_a_receiver,
    _learn_has_a_serial,
    _actions_have_channels,
)


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("keeloq")
    # In flash: the uint16_t counter, or what a remote without serial took over (8 bytes); the ESP8266 checks that all
    # saved values fit its store.
    preferences.reserve_preferences(1, 2 if CONF_SERIAL in config else 8, in_flash=True)
    # Only the derived device key reaches the firmware; the manufacturer key only for a remote without serial or with
    # on_frame (below). A remote without serial derives its device key when it takes a serial over.
    serial = config.get(CONF_SERIAL)
    manufacturer_key = config[CONF_MANUFACTURER_KEY]
    device_key = 0 if serial is None else normal_learning_key(serial, manufacturer_key)
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_TYPE],
        serial or 0,
        HexInt(device_key >> 32),
        HexInt(device_key & 0xFFFFFFFF),
    )
    await cg.register_component(var, config)
    if serial is None:
        # The take-over's preference key comes from the type and id only, so it stays the same across firmware updates.
        key = fnv1a_32bit_hash(f"keeloq.{config[CONF_TYPE]}.{config[CONF_ID]}")
        cg.add(var.set_take_over_key(key))
    cg.add_optional(var.set_channels, config.get(CONF_CHANNELS))
    if CONF_RECEIVER_ID in config:
        # The last accepted press, a uint16_t in flash.
        preferences.reserve_preferences(1, 2, in_flash=True)
        cg.add(var.set_receiving())
        # Frames of other serials and a hand remote's serial need their device key at run time; the remote's own
        # frames decrypt with the device key above.
        if serial is None or CONF_ON_FRAME in config:
            cg.add(
                var.set_manufacturer_key(
                    HexInt(manufacturer_key >> 32),
                    HexInt(manufacturer_key & 0xFFFFFFFF),
                )
            )
        await remote_base.register_listener(var, config)
    for conf in config.get(CONF_ON_FRAME, []):
        await automation.build_callback_automation(
            var, "add_on_frame_callback", _FRAME_ARGS, conf
        )
    for conf in config.get(CONF_ON_COMMAND, []):
        channel = conf.get(CONF_CHANNEL)
        command = conf.get(CONF_COMMAND)
        when = None
        if channel is not None or command is not None:
            channel = ANY_CHANNEL if channel is None else channel
            command = ANY_COMMAND if command is None else _command_code(command)
            when = f"keeloq::command_matches(x, {channel}, {command})"
        await automation.build_callback_automation(
            var,
            "add_on_command_callback",
            _FRAME_ARGS,
            conf,
            when=when,
        )


SEND_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(KeeloqRemote),
        cv.Required(CONF_COMMAND): cv.templatable(_command),
        # Without it, the remote's channels: configured, or taken over from a hand remote.
        cv.Optional(CONF_CHANNELS): cv.templatable(_channels),
    }
).extend(remote_base.BASE_REMOTE_TRANSMITTER_SCHEMA)

# The action's transmitter, passed to the remote as a pointer
_TRANSMITTER = (
    remote_base.CONF_TRANSMITTER_ID,
    remote_base.RemoteTransmitterBase.operator("ptr"),
)
_send_builder = automation.apply_action_builder(
    "keeloq.send",
    SEND_SCHEMA,
    automation.ApplyCall(
        "send({}, {}, {}, {}, {})",
        (
            _TRANSMITTER,
            (CONF_COMMAND, cg.uint8),
            (CONF_CHANNELS, cg.uint16),
            ((CONF_REPEAT, CONF_TIMES), cg.uint32),
            ((CONF_REPEAT, CONF_WAIT_TIME), cg.uint32),
        ),
    ),
)
# Without channels on the action: the remote's own when the action runs, configured or taken over
_send_own_channels_builder = automation.apply_action_builder(
    "keeloq.send",
    SEND_SCHEMA,
    automation.ApplyCall(
        "send({}, {}, {parent}->get_channels(), {}, {})",
        (
            _TRANSMITTER,
            (CONF_COMMAND, cg.uint8),
            ((CONF_REPEAT, CONF_TIMES), cg.uint32),
            ((CONF_REPEAT, CONF_WAIT_TIME), cg.uint32),
        ),
    ),
)


@automation.register_action(
    "keeloq.send", automation.ApplyAction, SEND_SCHEMA, synchronous=True
)
async def send_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    # A name brings its code and how often it is sent; repeat overrides that count.
    command = config[CONF_COMMAND]
    code, send_times = JAROLIFT_COMMANDS.get(command, (command, JAROLIFT_SEND_TIMES))
    repeat = config.get(CONF_REPEAT, {CONF_TIMES: send_times, CONF_WAIT_TIME: 0})
    config = {**config, CONF_COMMAND: code, CONF_REPEAT: repeat}
    if CONF_CHANNELS in config:
        return await _send_builder(config, action_id, template_arg, args)
    return await _send_own_channels_builder(config, action_id, template_arg, args)


automation.register_apply_action(
    "keeloq.set_counter",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(KeeloqRemote),
            cv.Required(CONF_COUNTER): cv.templatable(cv.uint16_t),
        }
    ),
    automation.ApplyField(CONF_COUNTER, "set_counter", cg.uint16),
)


LEARN_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(KeeloqRemote),
        # Without it, the remote's channels, as for keeloq.send.
        cv.Optional(CONF_CHANNELS): cv.templatable(_channels),
    }
).extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)
_learn_builder = automation.apply_action_builder(
    "keeloq.learn",
    LEARN_SCHEMA,
    automation.ApplyCall("learn({}, {})", (_TRANSMITTER, (CONF_CHANNELS, cg.uint16))),
)
_learn_own_channels_builder = automation.apply_action_builder(
    "keeloq.learn",
    LEARN_SCHEMA,
    automation.ApplyCall("learn({}, {parent}->get_channels())", (_TRANSMITTER,)),
)


@automation.register_action(
    "keeloq.learn", automation.ApplyAction, LEARN_SCHEMA, synchronous=True
)
async def learn_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    if CONF_CHANNELS in config:
        return await _learn_builder(config, action_id, template_arg, args)
    return await _learn_own_channels_builder(config, action_id, template_arg, args)


automation.register_apply_action(
    "keeloq.take_over",
    automation.maybe_simple_id({cv.GenerateID(): cv.use_id(KeeloqRemote)}),
    automation.ApplyCall("take_over_next_frame()"),
)
