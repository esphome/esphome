from esphome import automation
import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import (
    CONF_ASSUMED_STATE,
    CONF_CLOSE_ACTION,
    CONF_CURRENT_OPERATION,
    CONF_DEVICE_CLASS,
    CONF_ID,
    CONF_LAMBDA,
    CONF_OPEN_ACTION,
    CONF_OPTIMISTIC,
    CONF_POSITION,
    CONF_POSITION_ACTION,
    CONF_RESTORE_MODE,
    CONF_STATE,
    CONF_STOP_ACTION,
    CONF_TILT,
    CONF_TILT_ACTION,
    CONF_TILT_LAMBDA,
)

from .. import template_ns

TemplateCover = template_ns.class_("TemplateCover", cover.Cover, cg.Component)

TemplateCoverRestoreMode = template_ns.enum("TemplateCoverRestoreMode")
RESTORE_MODES = {
    "NO_RESTORE": TemplateCoverRestoreMode.COVER_NO_RESTORE,
    "RESTORE": TemplateCoverRestoreMode.COVER_RESTORE,
    "RESTORE_AND_CALL": TemplateCoverRestoreMode.COVER_RESTORE_AND_CALL,
}

CONF_HAS_POSITION = "has_position"
CONF_TOGGLE_ACTION = "toggle_action"

CONFIG_SCHEMA = (
    cv.with_visibility(
        cover.cover_schema(TemplateCover),
        cv.Visibility.UI,
        CONF_DEVICE_CLASS,
    )
    .extend(
        {
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
            cv.Optional(CONF_ASSUMED_STATE, default=False): cv.boolean,
            cv.Optional(CONF_HAS_POSITION, default=False): cv.boolean,
            cv.Optional(CONF_OPEN_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_STOP_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_TILT_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_TILT_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_TOGGLE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_POSITION_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_RESTORE_MODE, default="RESTORE"): cv.enum(
                RESTORE_MODES, upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(float)
        )
        cg.add(var.set_state_lambda(template_))
    if CONF_OPEN_ACTION in config:
        await automation.build_automation(
            var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
        )
    if CONF_CLOSE_ACTION in config:
        await automation.build_automation(
            var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
        )
    if CONF_STOP_ACTION in config:
        await automation.build_automation(
            var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
        )
        cg.add(var.set_has_stop(True))
    if CONF_TOGGLE_ACTION in config:
        await automation.build_automation(
            var.get_toggle_trigger(), [], config[CONF_TOGGLE_ACTION]
        )
        cg.add(var.set_has_toggle(True))
    if CONF_TILT_ACTION in config:
        await automation.build_automation(
            var.get_tilt_trigger(), [(float, "tilt")], config[CONF_TILT_ACTION]
        )
        cg.add(var.set_has_tilt(True))
    if CONF_TILT_LAMBDA in config:
        tilt_template_ = await cg.process_lambda(
            config[CONF_TILT_LAMBDA], [], return_type=cg.optional.template(float)
        )
        cg.add(var.set_tilt_lambda(tilt_template_))
    if CONF_POSITION_ACTION in config:
        await automation.build_automation(
            var.get_position_trigger(), [(float, "pos")], config[CONF_POSITION_ACTION]
        )
        cg.add(var.set_has_position(True))
    else:
        cg.add(var.set_has_position(config[CONF_HAS_POSITION]))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    cg.add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))
    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))


# CONF_STATE and CONF_POSITION are cv.Exclusive in the schema, so at most
# one is present and both map to the position field.
automation.register_apply_action(
    "cover.template.publish",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(cover.Cover),
            cv.Exclusive(CONF_STATE, "pos"): cv.templatable(cover.validate_cover_state),
            cv.Exclusive(CONF_POSITION, "pos"): cv.templatable(cv.zero_to_one_float),
            cv.Optional(CONF_CURRENT_OPERATION): cv.templatable(
                cover.validate_cover_operation
            ),
            cv.Optional(CONF_TILT): cv.templatable(cv.zero_to_one_float),
        }
    ),
    automation.ApplyField(CONF_STATE, "position = {}", cg.float_),
    automation.ApplyField(CONF_POSITION, "position = {}", cg.float_),
    automation.ApplyField(CONF_TILT, "tilt = {}", cg.float_),
    automation.ApplyField(
        CONF_CURRENT_OPERATION, "current_operation = {}", cover.CoverOperation
    ),
    automation.ApplyCall("publish_state()"),
)
