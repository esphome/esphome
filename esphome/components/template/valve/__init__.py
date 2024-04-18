import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import valve
from esphome.const import (
    CONF_ASSUMED_STATE,
    CONF_CLOSE_ACTION,
    CONF_CURRENT_OPERATION,
    CONF_ID,
    CONF_LAMBDA,
    CONF_OPEN_ACTION,
    CONF_OPTIMISTIC,
    CONF_POSITION,
    CONF_POSITION_ACTION,
    CONF_RESTORE_MODE,
    CONF_STATE,
    CONF_STOP_ACTION,
)
from .. import template_ns

TemplateValve = template_ns.class_("TemplateValve", valve.Valve, cg.Component)

TemplateValveRestoreMode = template_ns.enum("TemplateValveRestoreMode")
RESTORE_MODES = {
    "NO_RESTORE": TemplateValveRestoreMode.VALVE_NO_RESTORE,
    "RESTORE": TemplateValveRestoreMode.VALVE_RESTORE,
    "RESTORE_AND_CALL": TemplateValveRestoreMode.VALVE_RESTORE_AND_CALL,
}

CONF_HAS_POSITION = "has_position"
CONF_TOGGLE_ACTION = "toggle_action"

CONFIG_SCHEMA = valve.VALVE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TemplateValve),
        cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
        cv.Optional(CONF_ASSUMED_STATE, default=False): cv.boolean,
        cv.Optional(CONF_HAS_POSITION, default=False): cv.boolean,
        cv.Optional(CONF_OPEN_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_STOP_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_TOGGLE_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_POSITION_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_RESTORE_MODE, default="NO_RESTORE"): cv.enum(
            RESTORE_MODES, upper=True
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await valve.new_valve(config)
    await cg.register_component(var, config)
    if lambda_config := config.get(CONF_LAMBDA):
        template_ = await cg.process_lambda(
            lambda_config, [], return_type=cg.optional.template(float)
        )
        cg.add(var.set_state_lambda(template_))
    if open_action_config := config.get(CONF_OPEN_ACTION):
        await automation.build_automation(
            var.get_open_trigger(), [], open_action_config
        )
    if close_action_config := config.get(CONF_CLOSE_ACTION):
        await automation.build_automation(
            var.get_close_trigger(), [], close_action_config
        )
    if stop_action_config := config.get(CONF_STOP_ACTION):
        await automation.build_automation(
            var.get_stop_trigger(), [], stop_action_config
        )
        cg.add(var.set_has_stop(True))
    if toggle_action_config := config.get(CONF_TOGGLE_ACTION):
        await automation.build_automation(
            var.get_toggle_trigger(), [], toggle_action_config
        )
        cg.add(var.set_has_toggle(True))
    if position_action_config := config.get(CONF_POSITION_ACTION):
        await automation.build_automation(
            var.get_position_trigger(), [(float, "pos")], position_action_config
        )
        cg.add(var.set_has_position(True))
    else:
        cg.add(var.set_has_position(config[CONF_HAS_POSITION]))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    cg.add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))
    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))


@automation.register_action(
    "valve.template.publish",
    valve.ValvePublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(valve.Valve),
            cv.Exclusive(CONF_STATE, "pos"): cv.templatable(valve.validate_valve_state),
            cv.Exclusive(CONF_POSITION, "pos"): cv.templatable(cv.percentage),
            cv.Optional(CONF_CURRENT_OPERATION): cv.templatable(
                valve.validate_valve_operation
            ),
        }
    ),
)
async def valve_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if state_config := config.get(CONF_STATE):
        template_ = await cg.templatable(state_config, args, float)
        cg.add(var.set_position(template_))
    if (position_config := config.get(CONF_POSITION)) is not None:
        template_ = await cg.templatable(position_config, args, float)
        cg.add(var.set_position(template_))
    if current_operation_config := config.get(CONF_CURRENT_OPERATION):
        template_ = await cg.templatable(
            current_operation_config, args, valve.ValveOperation
        )
        cg.add(var.set_current_operation(template_))
    return var
