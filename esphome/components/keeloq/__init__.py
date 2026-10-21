from esphome import automation
import esphome.codegen as cg
from esphome.components import remote_base
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
from esphome.core import ID
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.helpers import fnv1a_32bit_hash
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["remote_transmitter"]
DOMAIN = "keeloq"
MULTI_CONF = True

CONF_COUNTER = "counter"
CONF_LEGACY = "legacy"
CONF_MANUFACTURER_KEY = "manufacturer_key"
CONF_ON_COMMAND = "on_command"
CONF_ON_FRAME = "on_frame"
CONF_SERIAL = "serial"

keeloq_ns = cg.esphome_ns.namespace("keeloq")
KeeloqRemote = keeloq_ns.class_("KeeloqRemote", cg.Component)
SendAction = keeloq_ns.class_("SendAction", remote_base.RemoteTransmitterActionBase)
LearnAction = keeloq_ns.class_("LearnAction", automation.Action)
KeeloqFrame = keeloq_ns.struct("KeeloqFrame")
_FRAME_ARGS = [(KeeloqFrame, "x")]
ANY_CHANNEL = 0
ANY_COMMAND = 0xFF

KeeloqType = keeloq_ns.enum("KeeloqType")
# A new learning variant adds its key derivation and frame layout in C++ and one entry here.
KEELOQ_TYPES = {
    "jarolift": KeeloqType.KEELOQ_TYPE_JAROLIFT,
}

# Per type: the button names as (code, send times), and None for the type's default send times
# (Jarolift: twice per press). Shade is stop held, sent 20 times. `repeat` on the action overrides.
TYPE_COMMANDS = {
    "jarolift": {
        "up": (0x8, None),
        "down": (0x2, None),
        "stop": (0x4, None),
        "learn": (0xA, None),
        "shade": (0x4, 20),
    },
}
JAROLIFT_COMMANDS = TYPE_COMMANDS["jarolift"]


def _manufacturer_key(value: object) -> int:
    value = cv.hex_int(value)
    if not 0 <= value <= 0xFFFFFFFFFFFFFFFF:
        raise cv.Invalid("manufacturer_key must be a 64-bit value")
    return value


def _command(value: object) -> int | str:
    if isinstance(value, str) and value.lower() in JAROLIFT_COMMANDS:
        return value.lower()
    try:
        return cv.int_range(min=0, max=0xF)(value)
    except cv.Invalid as err:
        raise cv.Invalid(
            f"command must be 0 to 0xF or one of {', '.join(JAROLIFT_COMMANDS)}"
        ) from err


def _command_code(command: int | str) -> int:
    return JAROLIFT_COMMANDS[command][0] if isinstance(command, str) else command


def _channels(value: object) -> int:
    """Channels 1 to 16 as a list, or the 16-bit mask they make (bit 0 is channel 1)."""
    if not isinstance(value, list):
        return cv.hex_uint16_t(value)
    channels = [cv.int_range(min=1, max=16)(channel) for channel in value]
    if len(set(channels)) != len(channels):
        raise cv.Invalid("each channel only once")
    return sum(1 << (channel - 1) for channel in channels)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(KeeloqRemote),
        cv.Required(CONF_TYPE): cv.enum(KEELOQ_TYPES, lower=True),
        cv.Required(CONF_MANUFACTURER_KEY): cv.sensitive(_manufacturer_key),
        # Without a serial the remote takes serial and counter over from a hand remote.
        cv.Optional(CONF_SERIAL): cv.All(cv.hex_int, cv.Range(min=0, max=0xFFFFFFF)),
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


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _frames_need_a_receiver)


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


def _take_over_has_a_receiver(config: ConfigType) -> ConfigType:
    # keeloq.take_over waits for a frame of the hand remote.
    if CONF_RECEIVER_ID in config:
        return config
    for path, action in _find_actions(fv.full_config.get(), [], ("keeloq.take_over",)):
        if str(action[CONF_ID]) != str(config[CONF_ID]):
            continue
        with cv.prepend_path([cv.ROOT_CONFIG_PATH] + path):
            raise cv.Invalid(f"keeloq.take_over needs receiver_id on {config[CONF_ID]}")
    return config


def _actions_have_channels(config: ConfigType) -> ConfigType:
    # A remote with serial has no hand remote to take channels from.
    if CONF_CHANNELS in config or CONF_SERIAL not in config:
        return config
    sending = ("keeloq.send", "keeloq.learn")
    for path, action in _find_actions(fv.full_config.get(), [], sending):
        if str(action[CONF_ID]) != str(config[CONF_ID]) or CONF_CHANNELS in action:
            continue
        with cv.prepend_path([cv.ROOT_CONFIG_PATH] + path):
            raise cv.Invalid(
                f"{action[CONF_ID]} has no channels; set channels on the action or the remote",
                [CONF_CHANNELS],
            )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(_take_over_has_a_receiver, _actions_have_channels)


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("keeloq")
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_TYPE],
        cg.RawExpression(f"0x{config[CONF_MANUFACTURER_KEY]:016X}ULL"),
        config.get(CONF_SERIAL, 0),
    )
    await cg.register_component(var, config)
    if CONF_SERIAL not in config:
        # The taken-over serial is stored under the type and id, which a new firmware keeps.
        key = fnv1a_32bit_hash(f"keeloq.{config[CONF_TYPE]}.{config[CONF_ID]}")
        cg.add(var.set_take_over_key(key))
    cg.add_optional(var.set_channels, config.get(CONF_CHANNELS))
    if CONF_RECEIVER_ID in config:
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


@automation.register_action("keeloq.send", SendAction, SEND_SCHEMA, synchronous=True)
async def send_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    remote = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, remote)
    await remote_base.register_transmittable(var, config)
    command = config[CONF_COMMAND]
    code, send_times = JAROLIFT_COMMANDS.get(command, (command, None))
    template_ = await cg.templatable(code, args, cg.uint8)
    cg.add(var.set_command(template_))
    if (channels := config.get(CONF_CHANNELS)) is not None:
        template_ = await cg.templatable(channels, args, cg.uint16)
        cg.add(var.set_channels(template_))
    if (repeat := config.get(CONF_REPEAT)) is not None:
        send_times = repeat[CONF_TIMES]
        template_ = await cg.templatable(repeat[CONF_WAIT_TIME], args, cg.uint32)
        cg.add(var.set_send_wait(template_))
    if send_times is not None:
        template_ = await cg.templatable(send_times, args, cg.uint32)
        cg.add(var.set_send_times(template_))
    return var


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


@automation.register_action(
    "keeloq.learn",
    LearnAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(KeeloqRemote),
            cv.Optional(CONF_CHANNELS): cv.templatable(_channels),
            cv.Optional(CONF_LEGACY): cv.templatable(cv.boolean),
        }
    ).extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA),
    synchronous=True,
)
async def learn_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    remote = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, remote)
    await remote_base.register_transmittable(var, config)
    if (channels := config.get(CONF_CHANNELS)) is not None:
        template_ = await cg.templatable(channels, args, cg.uint16)
        cg.add(var.set_channels(template_))
    if (legacy := config.get(CONF_LEGACY)) is not None:
        template_ = await cg.templatable(legacy, args, cg.bool_)
        cg.add(var.set_legacy(template_))
    return var


automation.register_apply_action(
    "keeloq.take_over",
    automation.maybe_simple_id({cv.GenerateID(): cv.use_id(KeeloqRemote)}),
    automation.ApplyCall("take_over_next_frame()"),
)
