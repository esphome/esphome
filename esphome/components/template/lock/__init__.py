import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import lock
from esphome.const import (
    CONF_ASSUMED_STATE,
    CONF_ID,
    CONF_LAMBDA,
    CONF_LOCK_ACTION,
    CONF_OPEN_ACTION,
    CONF_OPTIMISTIC,
    CONF_RESTORE_STATE,
    CONF_STATE,
    CONF_UNLOCK_ACTION,
)
from .. import template_ns

TemplateLock = template_ns.class_("TemplateLock", lock.Lock, cg.Component)


def validate(config):
    if not config[CONF_OPTIMISTIC] and (
        CONF_LOCK_ACTION not in config or CONF_UNLOCK_ACTION not in config
    ):
        raise cv.Invalid(
            "Either optimistic mode must be enabled, or turn_on_action and turn_off_action must be set, "
            "to handle the lock being changed."
        )
    return config


CONFIG_SCHEMA = cv.All(
    lock.LOCK_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TemplateLock),
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
            cv.Optional(CONF_ASSUMED_STATE, default=False): cv.boolean,
            cv.Optional(CONF_UNLOCK_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_LOCK_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_OPEN_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_RESTORE_STATE, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await lock.register_lock(var, config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(bool)
        )
        cg.add(var.set_state_lambda(template_))
    if CONF_UNLOCK_ACTION in config:
        await automation.build_automation(
            var.get_unlock_trigger(), [], config[CONF_UNLOCK_ACTION]
        )
    if CONF_LOCK_ACTION in config:
        await automation.build_automation(
            var.get_lock_trigger(), [], config[CONF_LOCK_ACTION]
        )
    if CONF_OPEN_ACTION in config:
        await automation.build_automation(
            var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
        )
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    cg.add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))
    cg.add(var.set_restore_state(config[CONF_RESTORE_STATE]))


@automation.register_action(
    "lock.template.publish",
    lock.LockPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(lock.Lock),
            cv.Required(CONF_STATE): cv.templatable(cv.boolean),
        }
    ),
)
async def lock_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))
    return var
