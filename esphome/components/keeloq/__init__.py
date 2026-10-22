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
from esphome.core import ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["remote_transmitter"]
DOMAIN = "keeloq"
MULTI_CONF = True

CONF_COUNTER = "counter"
CONF_LEGACY = "legacy"
CONF_MANUFACTURER_KEY = "manufacturer_key"
CONF_SERIAL = "serial"

keeloq_ns = cg.esphome_ns.namespace("keeloq")
KeeloqRemote = keeloq_ns.class_("KeeloqRemote", cg.Component)
SendAction = keeloq_ns.class_("SendAction", remote_base.RemoteTransmitterActionBase)

LearnAction = keeloq_ns.class_("LearnAction", automation.Action)

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


def _channels(value: object) -> int:
    """Channels 1 to 16 as a list, or the 16-bit mask they make (bit 0 is channel 1)."""
    if isinstance(value, list):
        channels = [cv.int_range(min=1, max=16)(channel) for channel in value]
        if len(set(channels)) != len(channels):
            raise cv.Invalid("each channel only once")
        value = sum(1 << (channel - 1) for channel in channels)
    mask = cv.hex_uint16_t(value)
    # No motor acts on a frame without a channel.
    if mask == 0:
        raise cv.Invalid("set at least one channel")
    return mask


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(KeeloqRemote),
        cv.Required(CONF_TYPE): cv.enum(KEELOQ_TYPES, lower=True),
        cv.Required(CONF_MANUFACTURER_KEY): cv.sensitive(_manufacturer_key),
        cv.Required(CONF_SERIAL): cv.All(cv.hex_int, cv.Range(min=0, max=0xFFFFFFF)),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("keeloq")
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_TYPE],
        cg.RawExpression(f"0x{config[CONF_MANUFACTURER_KEY]:016X}ULL"),
        config[CONF_SERIAL],
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
    code, send_times = JAROLIFT_COMMANDS.get(command, (command, None))
    template_ = await cg.templatable(code, args, cg.uint8)
    cg.add(var.set_command(template_))
    template_ = await cg.templatable(config[CONF_CHANNELS], args, cg.uint16)
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
            cv.Required(CONF_CHANNELS): cv.templatable(_channels),
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
    template_ = await cg.templatable(config[CONF_CHANNELS], args, cg.uint16)
    cg.add(var.set_channels(template_))
    if (legacy := config.get(CONF_LEGACY)) is not None:
        template_ = await cg.templatable(legacy, args, cg.bool_)
        cg.add(var.set_legacy(template_))
    return var
