from esphome import automation
import esphome.codegen as cg
from esphome.components import remote_base
import esphome.config_validation as cv
from esphome.const import (
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
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["remote_transmitter"]
DOMAIN = "keeloq"
MULTI_CONF = True

CONF_COUNTER = "counter"
CONF_MANUFACTURER_KEY = "manufacturer_key"
CONF_SERIAL = "serial"

keeloq_ns = cg.esphome_ns.namespace("keeloq")
KeeloqRemote = keeloq_ns.class_("KeeloqRemote", cg.Component)
SendAction = keeloq_ns.class_("SendAction", remote_base.RemoteTransmitterActionBase)

LearnAction = keeloq_ns.class_("LearnAction", automation.Action)

KeeloqType = keeloq_ns.enum("KeeloqType")
KEELOQ_TYPES = {
    "jarolift": KeeloqType.KEELOQ_TYPE_JAROLIFT,
}

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
        cv.Required(CONF_SERIAL): cv.hex_int_range(min=0, max=0xFFFFFFF),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("keeloq")
    # Only the derived device key reaches the firmware, never the manufacturer key.
    device_key = normal_learning_key(config[CONF_SERIAL], config[CONF_MANUFACTURER_KEY])
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_TYPE],
        config[CONF_SERIAL],
        HexInt(device_key >> 32),
        HexInt(device_key & 0xFFFFFFFF),
    )
    await cg.register_component(var, config)


SEND_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(KeeloqRemote),
        cv.Required(CONF_COMMAND): cv.templatable(_command),
        cv.Required(CONF_CHANNELS): cv.templatable(_channels),
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
    code, send_times = JAROLIFT_COMMANDS.get(command, (command, JAROLIFT_SEND_TIMES))
    template_ = await cg.templatable(code, args, cg.uint8)
    cg.add(var.set_command(template_))
    template_ = await cg.templatable(config[CONF_CHANNELS], args, cg.uint16)
    cg.add(var.set_channels(template_))
    if (repeat := config.get(CONF_REPEAT)) is not None:
        send_times = repeat[CONF_TIMES]
        template_ = await cg.templatable(repeat[CONF_WAIT_TIME], args, cg.uint32)
        cg.add(var.set_send_wait(template_))
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
            cv.Required(CONF_CHANNELS): cv.templatable(_channels),
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
    template_ = await cg.templatable(config[CONF_CHANNELS], args, cg.uint16)
    cg.add(var.set_channels(template_))
    return var
