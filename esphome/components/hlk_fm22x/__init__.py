import logging
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_ID,
    CONF_NAME,
    CONF_ON_ENROLLMENT_DONE,
    CONF_ON_ENROLLMENT_FAILED,
    CONF_TIMEOUT,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArguments
from esphome.types import ConfigType, TemplateArgsType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@OnFreund"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
MULTI_CONF = True

CONF_HLK_FM22X_ID = "hlk_fm22x_id"
CONF_FACE_ID = "face_id"
CONF_ADMIN = "admin"
CONF_ALLOW_DUPLICATE = "allow_duplicate"
CONF_ON_FACE_SCAN_MATCHED = "on_face_scan_matched"
CONF_ON_FACE_SCAN_UNMATCHED = "on_face_scan_unmatched"
CONF_ON_FACE_SCAN_INVALID = "on_face_scan_invalid"
CONF_ON_FACE_INFO = "on_face_info"
CONF_ON_FACE_DETAILS = "on_face_details"

ICON_FACE_RECOGNITION = "mdi:face-recognition"

# The module only talks at this speed; other rates can only be set in its firmware update mode
BAUD_RATE = 115200
# The module stores names in a 32 byte field; one byte is kept for the terminating NUL
MAX_NAME_LENGTH = 31

hlk_fm22x_ns = cg.esphome_ns.namespace("hlk_fm22x")
HlkFm22xComponent = hlk_fm22x_ns.class_(
    "HlkFm22xComponent", cg.Component, uart.UARTDevice
)
HlkFm22xFaceDirection = hlk_fm22x_ns.enum("HlkFm22xFaceDirection")

EnrollmentAction = hlk_fm22x_ns.class_("EnrollmentAction", automation.Action)
ScanAction = hlk_fm22x_ns.class_("ScanAction", automation.Action)
CancelAction = hlk_fm22x_ns.class_("CancelAction", automation.Action)
DeleteAction = hlk_fm22x_ns.class_("DeleteAction", automation.Action)
DeleteAllAction = hlk_fm22x_ns.class_("DeleteAllAction", automation.Action)
GetFaceDetailsAction = hlk_fm22x_ns.class_("GetFaceDetailsAction", automation.Action)
ResetAction = hlk_fm22x_ns.class_("ResetAction", automation.Action)

FACE_DIRECTIONS = {
    "middle": HlkFm22xFaceDirection.FACE_DIRECTION_MIDDLE,
    "right": HlkFm22xFaceDirection.FACE_DIRECTION_RIGHT,
    "left": HlkFm22xFaceDirection.FACE_DIRECTION_LEFT,
    "down": HlkFm22xFaceDirection.FACE_DIRECTION_DOWN,
    "up": HlkFm22xFaceDirection.FACE_DIRECTION_UP,
}

# Scans and enrollments accept a timeout of 1 to 255 seconds
TIMEOUT_SCHEMA = cv.All(
    cv.positive_time_period_seconds,
    cv.Range(min=cv.TimePeriod(seconds=1), max=cv.TimePeriod(seconds=255)),
)


def validate_direction(value: Any) -> Any:
    """Accept a direction name or the module's own numeric code."""
    if isinstance(value, str):
        return cv.enum(FACE_DIRECTIONS, lower=True)(value)
    return cv.uint8_t(value)


def validate_name(value: Any) -> str:
    value = cv.string(value)
    if len(value.encode("utf-8")) > MAX_NAME_LENGTH:
        raise cv.Invalid(f"Names are limited to {MAX_NAME_LENGTH} bytes of ASCII text")
    return value


def _remove_update_interval(config: Any) -> Any:
    # Remove before 2027.3.0
    if isinstance(config, dict) and CONF_UPDATE_INTERVAL in config:
        _LOGGER.warning(
            "[hlk_fm22x] 'update_interval' is no longer used because the module is "
            "read continuously. Will be removed in 2027.3.0"
        )
        config = config.copy()
        del config[CONF_UPDATE_INTERVAL]
    return config


CONFIG_SCHEMA = cv.All(
    _remove_update_interval,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HlkFm22xComponent),
            cv.Optional(CONF_ON_FACE_SCAN_MATCHED): automation.validate_automation({}),
            cv.Optional(CONF_ON_FACE_SCAN_UNMATCHED): automation.validate_automation(
                {}
            ),
            cv.Optional(CONF_ON_FACE_SCAN_INVALID): automation.validate_automation({}),
            cv.Optional(CONF_ON_FACE_INFO): automation.validate_automation({}),
            cv.Optional(CONF_ON_FACE_DETAILS): automation.validate_automation({}),
            cv.Optional(CONF_ON_ENROLLMENT_DONE): automation.validate_automation({}),
            cv.Optional(CONF_ON_ENROLLMENT_FAILED): automation.validate_automation({}),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "hlk_fm22x", baud_rate=BAUD_RATE, require_tx=True, require_rx=True
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_FACE_SCAN_MATCHED,
        "add_on_face_scan_matched_callback",
        [(cg.int16, "face_id"), (cg.std_string, "name"), (cg.bool_, "admin")],
    ),
    automation.CallbackAutomation(
        CONF_ON_FACE_SCAN_UNMATCHED, "add_on_face_scan_unmatched_callback"
    ),
    automation.CallbackAutomation(
        CONF_ON_FACE_SCAN_INVALID,
        "add_on_face_scan_invalid_callback",
        [(cg.uint8, "error")],
    ),
    automation.CallbackAutomation(
        CONF_ON_FACE_INFO,
        "add_on_face_info_callback",
        [
            (cg.int16, "status"),
            (cg.int16, "left"),
            (cg.int16, "top"),
            (cg.int16, "right"),
            (cg.int16, "bottom"),
            (cg.int16, "yaw"),
            (cg.int16, "pitch"),
            (cg.int16, "roll"),
        ],
    ),
    automation.CallbackAutomation(
        CONF_ON_FACE_DETAILS,
        "add_on_face_details_callback",
        [(cg.int16, "face_id"), (cg.std_string, "name"), (cg.bool_, "admin")],
    ),
    automation.CallbackAutomation(
        CONF_ON_ENROLLMENT_DONE,
        "add_on_enrollment_done_callback",
        [(cg.int16, "face_id"), (cg.uint8, "direction")],
    ),
    automation.CallbackAutomation(
        CONF_ON_ENROLLMENT_FAILED,
        "add_on_enrollment_failed_callback",
        [(cg.uint8, "error")],
    ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


async def _new_action(
    config: ConfigType, action_id: ID, template_arg: TemplateArguments
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "hlk_fm22x.enroll",
    EnrollmentAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Required(CONF_NAME): cv.templatable(validate_name),
            cv.Optional(CONF_DIRECTION): cv.templatable(validate_direction),
            cv.Optional(CONF_ADMIN, default=False): cv.templatable(cv.boolean),
            cv.Optional(CONF_TIMEOUT, default="10s"): TIMEOUT_SCHEMA,
            cv.Optional(CONF_ALLOW_DUPLICATE, default=True): cv.templatable(cv.boolean),
        },
        key=CONF_NAME,
    ),
    synchronous=True,
)
async def hlk_fm22x_enroll_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = await _new_action(config, action_id, template_arg)
    template_ = await cg.templatable(config[CONF_NAME], args, cg.std_string)
    cg.add(var.set_name(template_))
    # Without a direction the face is enrolled from a single frame
    if (direction := config.get(CONF_DIRECTION)) is not None:
        template_ = await cg.templatable(direction, args, cg.uint8)
        cg.add(var.set_direction(template_))
    template_ = await cg.templatable(config[CONF_ADMIN], args, cg.bool_)
    cg.add(var.set_admin(template_))
    template_ = await cg.templatable(config[CONF_ALLOW_DUPLICATE], args, cg.bool_)
    cg.add(var.set_allow_duplicate(template_))
    cg.add(var.set_timeout_seconds(int(config[CONF_TIMEOUT].total_seconds)))
    return var


@automation.register_action(
    "hlk_fm22x.scan",
    ScanAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Optional(CONF_TIMEOUT, default="10s"): TIMEOUT_SCHEMA,
        }
    ),
    synchronous=True,
)
async def hlk_fm22x_scan_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = await _new_action(config, action_id, template_arg)
    cg.add(var.set_timeout_seconds(int(config[CONF_TIMEOUT].total_seconds)))
    return var


@automation.register_action(
    "hlk_fm22x.cancel",
    CancelAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
        }
    ),
    synchronous=True,
)
async def hlk_fm22x_cancel_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _new_action(config, action_id, template_arg)


@automation.register_action(
    "hlk_fm22x.delete",
    DeleteAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Required(CONF_FACE_ID): cv.templatable(cv.int_range(min=0, max=32767)),
        },
        key=CONF_FACE_ID,
    ),
    synchronous=True,
)
async def hlk_fm22x_delete_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = await _new_action(config, action_id, template_arg)
    template_ = await cg.templatable(config[CONF_FACE_ID], args, cg.int16)
    cg.add(var.set_face_id(template_))
    return var


@automation.register_action(
    "hlk_fm22x.delete_all",
    DeleteAllAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
        }
    ),
    synchronous=True,
)
async def hlk_fm22x_delete_all_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _new_action(config, action_id, template_arg)


@automation.register_action(
    "hlk_fm22x.get_face_details",
    GetFaceDetailsAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Required(CONF_FACE_ID): cv.templatable(cv.int_range(min=0, max=32767)),
        },
        key=CONF_FACE_ID,
    ),
    synchronous=True,
)
async def hlk_fm22x_get_face_details_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = await _new_action(config, action_id, template_arg)
    template_ = await cg.templatable(config[CONF_FACE_ID], args, cg.int16)
    cg.add(var.set_face_id(template_))
    return var


@automation.register_action(
    "hlk_fm22x.reset",
    ResetAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
        }
    ),
    synchronous=True,
)
async def hlk_fm22x_reset_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _new_action(config, action_id, template_arg)
