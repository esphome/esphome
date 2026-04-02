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

EnrollmentAction = hlk_fm22x_ns.class_("EnrollmentAction", automation.Action)
DeleteAction = hlk_fm22x_ns.class_("DeleteAction", automation.Action)
DeleteAllAction = hlk_fm22x_ns.class_("DeleteAllAction", automation.Action)
ScanAction = hlk_fm22x_ns.class_("ScanAction", automation.Action)
ResetAction = hlk_fm22x_ns.class_("ResetAction", automation.Action)

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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    for conf in config.get(CONF_ON_FACE_SCAN_MATCHED, []):
        await automation.build_callback_automation(
            var,
            "add_on_face_scan_matched_callback",
            [(cg.int16, "face_id"), (cg.std_string, "name")],
            conf,
        )

    for conf in config.get(CONF_ON_FACE_SCAN_UNMATCHED, []):
        await automation.build_callback_automation(
            var, "add_on_face_scan_unmatched_callback", [], conf
        )

    for conf in config.get(CONF_ON_FACE_SCAN_INVALID, []):
        await automation.build_callback_automation(
            var, "add_on_face_scan_invalid_callback", [(cg.uint8, "error")], conf
        )

    for conf in config.get(CONF_ON_FACE_INFO, []):
        await automation.build_callback_automation(
            var,
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
            conf,
        )

    for conf in config.get(CONF_ON_ENROLLMENT_DONE, []):
        await automation.build_callback_automation(
            var,
            "add_on_enrollment_done_callback",
            [(cg.int16, "face_id"), (cg.uint8, "direction")],
            conf,
        )

    for conf in config.get(CONF_ON_ENROLLMENT_FAILED, []):
        await automation.build_callback_automation(
            var, "add_on_enrollment_failed_callback", [(cg.uint8, "error")], conf
        )


@automation.register_action(
    "hlk_fm22x.enroll",
    EnrollmentAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Required(CONF_NAME): cv.templatable(cv.string),
            cv.Required(CONF_DIRECTION): cv.templatable(cv.uint8_t),
        },
        key=CONF_NAME,
    ),
    synchronous=True,
)
async def hlk_fm22x_enroll_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_ = await cg.templatable(config[CONF_NAME], args, cg.std_string)
    cg.add(var.set_name(template_))
    template_ = await cg.templatable(config[CONF_DIRECTION], args, cg.uint8)
    cg.add(var.set_direction(template_))
    return var


@automation.register_action(
    "hlk_fm22x.delete",
    DeleteAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
            cv.Required(CONF_FACE_ID): cv.templatable(cv.uint16_t),
        },
        key=CONF_FACE_ID,
    ),
    synchronous=True,
)
async def hlk_fm22x_delete_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

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
async def hlk_fm22x_delete_all_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "hlk_fm22x.scan",
    ScanAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlkFm22xComponent),
        }
    ),
    synchronous=True,
)
async def hlk_fm22x_scan_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
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
async def hlk_fm22x_reset_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
