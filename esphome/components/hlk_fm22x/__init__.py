import logging
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_NAME,
    CONF_ON_ENROLLMENT_DONE,
    CONF_ON_ENROLLMENT_FAILED,
    CONF_TIMEOUT,
    CONF_UPDATE_INTERVAL,
)
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@OnFreund"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
DOMAIN = "hlk_fm22x"
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

# Trigger strings are passed by const reference so nothing is copied per call
STRING_ARG = cg.std_string_ref.operator("const")

hlk_fm22x_ns = cg.esphome_ns.namespace("hlk_fm22x")
HlkFm22xComponent = hlk_fm22x_ns.class_(
    "HlkFm22xComponent", cg.Component, uart.UARTDevice
)
HlkFm22xFaceDirection = hlk_fm22x_ns.enum("HlkFm22xFaceDirection")

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
    # A matched name is trimmed to printable ASCII on the way back, so accept nothing else
    if len(value) > MAX_NAME_LENGTH or not (value.isascii() and value.isprintable()):
        raise cv.Invalid(
            f"Names are limited to {MAX_NAME_LENGTH} printable ASCII characters"
        )
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
        [(cg.int16, "face_id"), (STRING_ARG, "name"), (cg.bool_, "admin")],
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
        [(cg.int16, "face_id"), (STRING_ARG, "name"), (cg.bool_, "admin")],
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
    var = await uart.new_uart_component(config)

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


automation.register_apply_action(
    "hlk_fm22x.enroll",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Required(CONF_NAME): cv.templatable(validate_name),
            # Direction 0 (the default) enrolls the face from a single frame
            cv.Optional(CONF_DIRECTION, default=0): cv.templatable(validate_direction),
            cv.Optional(CONF_ADMIN, default=False): cv.templatable(cv.boolean),
            cv.Optional(CONF_TIMEOUT, default="10s"): TIMEOUT_SCHEMA,
            cv.Optional(CONF_ALLOW_DUPLICATE, default=True): cv.templatable(cv.boolean),
        },
        key=CONF_NAME,
    ),
    automation.ApplyCall(
        "enroll_face({}, static_cast<hlk_fm22x::HlkFm22xFaceDirection>({}), {}, {}, {})",
        (
            (CONF_NAME, cg.std_string),
            (CONF_DIRECTION, cg.uint8),
            (CONF_ADMIN, cg.bool_),
            (CONF_TIMEOUT, cg.uint8),
            (CONF_ALLOW_DUPLICATE, cg.bool_),
        ),
    ),
)


automation.register_apply_action(
    "hlk_fm22x.scan",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Optional(CONF_TIMEOUT, default="10s"): TIMEOUT_SCHEMA,
        }
    ),
    automation.ApplyField(CONF_TIMEOUT, "scan_face", cg.uint8),
)

automation.register_apply_action(
    "hlk_fm22x.cancel",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
        }
    ),
    automation.ApplyCall("cancel()"),
)


automation.register_apply_action(
    "hlk_fm22x.delete",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Required(CONF_FACE_ID): cv.templatable(cv.int_range(min=0, max=32767)),
        },
        key=CONF_FACE_ID,
    ),
    automation.ApplyField(CONF_FACE_ID, "delete_face", cg.int16),
)

automation.register_apply_action(
    "hlk_fm22x.delete_all",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
        }
    ),
    automation.ApplyCall("delete_all_faces()"),
)

automation.register_apply_action(
    "hlk_fm22x.get_face_details",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Required(CONF_FACE_ID): cv.templatable(cv.int_range(min=0, max=32767)),
        },
        key=CONF_FACE_ID,
    ),
    automation.ApplyField(CONF_FACE_ID, "get_face_details", cg.int16),
)


automation.register_apply_action(
    "hlk_fm22x.reset",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
        }
    ),
    automation.ApplyCall("reset()"),
)
