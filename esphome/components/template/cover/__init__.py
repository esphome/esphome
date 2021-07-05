import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import cover
from esphome.const import (
    CONF_ASSUMED_STATE,
    CONF_CLOSE_ACTION,
    CONF_CURRENT_OPERATION,
    CONF_ID,
    CONF_LAMBDA,
    CONF_OPEN_ACTION,
    CONF_OPTIMISTIC,
    CONF_POSITION,
    CONF_RESTORE_MODE,
    CONF_STATE,
    CONF_STOP_ACTION,
    CONF_TILT,
    CONF_TILT_ACTION,
    CONF_TILT_LAMBDA,
    CONF_POSITION_ACTION,
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

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TemplateCover),
        cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
        cv.Optional(CONF_ASSUMED_STATE, default=False): cv.boolean,
        cv.Optional(CONF_HAS_POSITION, default=False): cv.boolean,
        cv.Optional(CONF_OPEN_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_STOP_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_TILT_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_TILT_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_POSITION_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_RESTORE_MODE, default="RESTORE"): cv.enum(
            RESTORE_MODES, upper=True
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
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
    cg.add(var.set_has_position(config[CONF_HAS_POSITION]))


@automation.register_action(
    "cover.template.publish",
    cover.CoverPublishAction,
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
)
async def cover_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_STATE in config:
        template_ = await cg.templatable(config[CONF_STATE], args, float)
        cg.add(var.set_position(template_))
    if CONF_POSITION in config:
        template_ = await cg.templatable(config[CONF_POSITION], args, float)
        cg.add(var.set_position(template_))
    if CONF_TILT in config:
        template_ = await cg.templatable(config[CONF_TILT], args, float)
        cg.add(var.set_tilt(template_))
    if CONF_CURRENT_OPERATION in config:
        template_ = await cg.templatable(
            config[CONF_CURRENT_OPERATION], args, cover.CoverOperation
        )
        cg.add(var.set_current_operation(template_))
    return var
