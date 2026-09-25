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
)

CODEOWNERS = ["@OnFreund"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
MULTI_CONF = True

CONF_HLK_FM22X_ID = "hlk_fm22x_id"
CONF_FACE_ID = "face_id"
CONF_ON_FACE_SCAN_MATCHED = "on_face_scan_matched"
CONF_ON_FACE_SCAN_UNMATCHED = "on_face_scan_unmatched"
CONF_ON_FACE_SCAN_INVALID = "on_face_scan_invalid"
CONF_ON_FACE_INFO = "on_face_info"

hlk_fm22x_ns = cg.esphome_ns.namespace("hlk_fm22x")
HlkFm22xComponent = hlk_fm22x_ns.class_(
    "HlkFm22xComponent", cg.PollingComponent, uart.UARTDevice
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HlkFm22xComponent),
            cv.Optional(CONF_ON_FACE_SCAN_MATCHED): automation.validate_automation({}),
            cv.Optional(CONF_ON_FACE_SCAN_UNMATCHED): automation.validate_automation(
                {}
            ),
            cv.Optional(CONF_ON_FACE_SCAN_INVALID): automation.validate_automation({}),
            cv.Optional(CONF_ON_FACE_INFO): automation.validate_automation({}),
            cv.Optional(CONF_ON_ENROLLMENT_DONE): automation.validate_automation({}),
            cv.Optional(CONF_ON_ENROLLMENT_FAILED): automation.validate_automation({}),
        }
    )
    .extend(cv.polling_component_schema("50ms"))
    .extend(uart.UART_DEVICE_SCHEMA),
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_FACE_SCAN_MATCHED,
        "add_on_face_scan_matched_callback",
        [(cg.int16, "face_id"), (cg.std_string, "name")],
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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


automation.register_apply_action(
    "hlk_fm22x.enroll",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Required(CONF_NAME): cv.templatable(cv.string),
            cv.Required(CONF_DIRECTION): cv.templatable(cv.uint8_t),
        },
        key=CONF_NAME,
    ),
    automation.ApplyCall(
        "enroll_face({}, static_cast<hlk_fm22x::HlkFm22xFaceDirection>({}))",
        ((CONF_NAME, cg.std_string), (CONF_DIRECTION, cg.uint8)),
    ),
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
    "hlk_fm22x.scan",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
        }
    ),
    automation.ApplyCall("scan_face()"),
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
