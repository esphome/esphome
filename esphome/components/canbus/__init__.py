import logging
import re
from typing import Any

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_ID, CONF_PLATFORM, CONF_TRIGGER_ID
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@mvturnho", "@danielschramm"]
DOMAIN = "canbus"
IS_PLATFORM_COMPONENT = True

CONF_CAN_ID = "can_id"
CONF_CAN_ID_MASK = "can_id_mask"
CONF_USE_EXTENDED_ID = "use_extended_id"
CONF_REMOTE_TRANSMISSION_REQUEST = "remote_transmission_request"
CONF_CANBUS_ID = "canbus_id"
CONF_BIT_RATE = "bit_rate"
CONF_ON_FRAME = "on_frame"
CONF_MAX_FRAMES_PER_LOOP = "max_frames_per_loop"
CONF_FD = "fd"
CONF_BIT_RATE_SWITCH = "bit_rate_switch"
CONF_PAD_DATA = "pad_data"
CONF_DEFAULT_FD = "default_fd"
CONF_DEFAULT_BIT_RATE_SWITCH = "default_bit_rate_switch"
CONF_DEFAULT_PAD_DATA = "default_pad_data"
DEFAULT_MAX_FRAMES_PER_LOOP = 50  # Keep in sync with max_frames_per_loop_ in canbus.h
DEFAULT_BIT_RATE = "125KBPS"  # Keep in sync with bit_rate_ in canbus.h


def validate_id(config: ConfigType) -> ConfigType:
    if CONF_CAN_ID in config:
        can_id = config[CONF_CAN_ID]
        id_ext = config[CONF_USE_EXTENDED_ID]
        if not id_ext and can_id > 0x7FF:
            raise cv.Invalid("Standard IDs must be 11 Bit (0x000-0x7ff / 0-2047)")
    return config


def validate_raw_data(value: Any) -> bytes | list:
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


canbus_ns = cg.esphome_ns.namespace("canbus")
CanbusComponent = canbus_ns.class_("CanbusComponent", cg.Component)
CanbusTrigger = canbus_ns.class_(
    "CanbusTrigger",
    automation.Trigger.template(cg.std_vector.template(cg.uint8), cg.uint32),
    cg.Component,
)
CanSpeed = canbus_ns.enum("CAN_SPEED")

CAN_SPEEDS = CanSpeed.options(
    "1KBPS",
    "5KBPS",
    "10KBPS",
    "12K5BPS",
    "16KBPS",
    "20KBPS",
    "25KBPS",
    "31K25BPS",
    "33KBPS",
    "40KBPS",
    "50KBPS",
    "80KBPS",
    "83K3BPS",
    "95KBPS",
    "100KBPS",
    "125KBPS",
    "200KBPS",
    "250KBPS",
    "500KBPS",
    "800KBPS",
    "1000KBPS",
    prefix="CAN_",
)

CAN_DATA_SPEEDS = {
    **CAN_SPEEDS,
    "2000KBPS": CanSpeed.CAN_2000KBPS,
    "3000KBPS": CanSpeed.CAN_3000KBPS,
    "4000KBPS": CanSpeed.CAN_4000KBPS,
    "5000KBPS": CanSpeed.CAN_5000KBPS,
    "6000KBPS": CanSpeed.CAN_6000KBPS,
    "7000KBPS": CanSpeed.CAN_7000KBPS,
    "8000KBPS": CanSpeed.CAN_8000KBPS,
}


def get_rate(value: str) -> int:
    match = re.match(r"(\d+)(?:K(\d+)?)?BPS", value, re.IGNORECASE)
    if not match:
        raise ValueError(f"Invalid rate format: {value}")
    fraction = match.group(2) or "0"
    return int((float(match.group(1)) + float(f"0.{fraction}")) * 1000)


CANBUS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CanbusComponent),
        cv.Required(CONF_CAN_ID): cv.int_range(min=0, max=0x1FFFFFFF),
        cv.Optional(CONF_BIT_RATE, default=DEFAULT_BIT_RATE): cv.enum(
            CAN_SPEEDS, upper=True
        ),
        cv.Optional(CONF_USE_EXTENDED_ID, default=False): cv.boolean,
        cv.Optional(CONF_DEFAULT_FD, default=False): cv.boolean_false,
        cv.Optional(CONF_DEFAULT_BIT_RATE_SWITCH, default=False): cv.boolean_false,
        cv.Optional(CONF_DEFAULT_PAD_DATA, default=False): cv.boolean,
        cv.Optional(
            CONF_MAX_FRAMES_PER_LOOP, default=DEFAULT_MAX_FRAMES_PER_LOOP
        ): cv.positive_not_null_int,
        cv.Optional(CONF_ON_FRAME): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CanbusTrigger),
                cv.Required(CONF_CAN_ID): cv.int_range(min=0, max=0x1FFFFFFF),
                cv.Optional(CONF_CAN_ID_MASK, default=0x1FFFFFFF): cv.int_range(
                    min=0, max=0x1FFFFFFF
                ),
                cv.Optional(CONF_USE_EXTENDED_ID, default=False): cv.boolean,
                cv.Optional(CONF_REMOTE_TRANSMISSION_REQUEST): cv.boolean,
            },
            validate_id,
        ),
    },
).extend(cv.COMPONENT_SCHEMA)

CANBUS_SCHEMA.add_extra(validate_id)


async def setup_canbus_core_(var: MockObj, config: ConfigType) -> None:
    await cg.register_component(var, config)
    cg.add(var.set_can_id(config[CONF_CAN_ID]))
    if config[CONF_USE_EXTENDED_ID]:
        cg.add(var.set_use_extended_id(True))
    if config[CONF_DEFAULT_FD]:
        cg.add(var.set_default_fd(True))
    if config[CONF_DEFAULT_BIT_RATE_SWITCH]:
        cg.add(var.set_default_bit_rate_switch(True))
    if config[CONF_DEFAULT_PAD_DATA]:
        cg.add(var.set_default_pad_data(True))
    if (bit_rate := config[CONF_BIT_RATE]) != DEFAULT_BIT_RATE:
        cg.add(var.set_bitrate(CAN_SPEEDS[bit_rate]))
    if (max_frames := config[CONF_MAX_FRAMES_PER_LOOP]) != DEFAULT_MAX_FRAMES_PER_LOOP:
        cg.add(var.set_max_frames_per_loop(max_frames))

    for conf in config.get(CONF_ON_FRAME, []):
        can_id = conf[CONF_CAN_ID]
        can_id_mask = conf[CONF_CAN_ID_MASK]
        ext_id = conf[CONF_USE_EXTENDED_ID]
        trigger = cg.new_Pvariable(
            conf[CONF_TRIGGER_ID], var, can_id, can_id_mask, ext_id
        )
        if CONF_REMOTE_TRANSMISSION_REQUEST in conf:
            cg.add(
                trigger.set_remote_transmission_request(
                    conf[CONF_REMOTE_TRANSMISSION_REQUEST]
                )
            )
        await cg.register_component(trigger, conf)
        await automation.build_automation(
            trigger,
            [
                (cg.std_vector.template(cg.uint8), "x"),
                (cg.uint32, "can_id"),
                (cg.bool_, "remote_transmission_request"),
            ],
            conf,
        )


async def register_canbus(
    var: MockObj, config: ConfigType, *, reports_status: bool | None = None
) -> None:
    if not CORE.has_id(config[CONF_ID]):
        var = cg.new_Pvariable(config[CONF_ID], var)
    if reports_status is None:
        _LOGGER.warning(
            "CAN bus platform '%s' does not report bus state, so bus-off recovery and "
            "error-state logging are disabled. Platform authors should either override "
            "get_status() and get_events() and call register_canbus(..., reports_status=True) "
            "or pass reports_status=False if the hardware cannot report it",
            config[CONF_PLATFORM],
        )
    await setup_canbus_core_(var, config)


def validate_send(config: ConfigType) -> ConfigType:
    """Reject statically invalid frames without resolving inherited or templated flags."""
    fd = config.get(CONF_FD)
    if fd is False and config.get(CONF_BIT_RATE_SWITCH) is True:
        raise cv.Invalid("bit_rate_switch requires fd: true")
    if fd is True and config[CONF_REMOTE_TRANSMISSION_REQUEST]:
        raise cv.Invalid("remote_transmission_request cannot be used with fd: true")

    data = config[CONF_DATA]
    if cg.is_template(data):
        return config
    if fd is False and len(data) > 8:
        raise cv.Invalid("Classic CAN data must be at most 8 bytes")
    pad_data = config.get(CONF_PAD_DATA)
    if (
        fd is True
        and len(data) > 8
        and len(data) not in (12, 16, 20, 24, 32, 48, 64)
        and pad_data is False
    ):
        raise cv.Invalid("CAN FD data length requires pad_data: true")
    return config


# Actions
CANBUS_SEND_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
        cv.Optional(CONF_CAN_ID): cv.int_range(min=0, max=0x1FFFFFFF),
        cv.Optional(CONF_USE_EXTENDED_ID, default=False): cv.boolean,
        cv.Optional(CONF_REMOTE_TRANSMISSION_REQUEST, default=False): cv.boolean,
        cv.Optional(CONF_FD): cv.templatable(cv.boolean),
        cv.Optional(CONF_BIT_RATE_SWITCH): cv.templatable(cv.boolean),
        cv.Optional(CONF_PAD_DATA): cv.templatable(cv.boolean),
        cv.Required(CONF_DATA): cv.templatable(
            cv.All(validate_raw_data, cv.Length(max=64))
        ),
    },
    validate_id,
    validate_send,
    key=CONF_DATA,
)


@automation.register_action(
    "canbus.send",
    canbus_ns.class_("CanbusSendAction", automation.Action),
    CANBUS_SEND_ACTION_SCHEMA,
    synchronous=True,
)
async def canbus_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_CANBUS_ID])

    if (can_id := config.get(CONF_CAN_ID)) is not None:
        cg.add(var.set_can_id(can_id))
        cg.add(var.set_use_extended_id(config[CONF_USE_EXTENDED_ID]))

    cg.add(
        var.set_remote_transmission_request(config[CONF_REMOTE_TRANSMISSION_REQUEST])
    )
    if (fd := config.get(CONF_FD)) is not None:
        cg.add(var.set_fd(await cg.templatable(fd, args, cg.bool_)))
    if (bit_rate_switch := config.get(CONF_BIT_RATE_SWITCH)) is not None:
        cg.add(
            var.set_bit_rate_switch(
                await cg.templatable(bit_rate_switch, args, cg.bool_)
            )
        )
    if (pad_data := config.get(CONF_PAD_DATA)) is not None:
        cg.add(var.set_pad_data(await cg.templatable(pad_data, args, cg.bool_)))

    await automation.templatable_bytes(
        config[CONF_DATA],
        args,
        var.set_data_template,
        var.set_data_static,
        "canbus_data",
    )
    return var
