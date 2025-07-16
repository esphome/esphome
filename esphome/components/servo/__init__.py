from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components.output import FloatOutput
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_IDLE_LEVEL,
    CONF_LEVEL,
    CONF_MAX_LEVEL,
    CONF_MIN_LEVEL,
    CONF_OUTPUT,
    CONF_RESTORE,
    CONF_TRANSITION_LENGTH,
)

servo_ns = cg.esphome_ns.namespace("servo")
Servo = servo_ns.class_("Servo", cg.Component)
ServoWriteAction = servo_ns.class_("ServoWriteAction", automation.Action)
ServoDetachAction = servo_ns.class_("ServoDetachAction", automation.Action)

CONF_AUTO_DETACH_TIME = "auto_detach_time"
MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Servo),
        cv.Required(CONF_OUTPUT): cv.use_id(FloatOutput),
        cv.Optional(CONF_MIN_LEVEL, default="3%"): cv.percentage,
        cv.Optional(CONF_IDLE_LEVEL, default="7.5%"): cv.percentage,
        cv.Optional(CONF_MAX_LEVEL, default="12%"): cv.percentage,
        cv.Optional(CONF_RESTORE, default=False): cv.boolean,
        cv.Optional(
            CONF_AUTO_DETACH_TIME, default="0s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_TRANSITION_LENGTH, default="0s"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    out = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))
    cg.add(var.set_min_level(config[CONF_MIN_LEVEL]))
    cg.add(var.set_idle_level(config[CONF_IDLE_LEVEL]))
    cg.add(var.set_max_level(config[CONF_MAX_LEVEL]))
    cg.add(var.set_restore(config[CONF_RESTORE]))
    cg.add(var.set_auto_detach_time(config[CONF_AUTO_DETACH_TIME]))
    cg.add(var.set_transition_length(config[CONF_TRANSITION_LENGTH]))


@automation.register_action(
    "servo.write",
    ServoWriteAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(Servo),
            cv.Required(CONF_LEVEL): cv.templatable(cv.possibly_negative_percentage),
        }
    ),
)
async def servo_write_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_LEVEL], args, float)
    cg.add(var.set_value(template_))
    return var


@automation.register_action(
    "servo.detach",
    ServoDetachAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Servo),
        }
    ),
)
async def servo_detach_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
