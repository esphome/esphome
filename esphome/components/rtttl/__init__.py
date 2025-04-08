import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components.output import FloatOutput
from esphome.components.speaker import Speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_GAIN,
    CONF_ID,
    CONF_OUTPUT,
    CONF_PLATFORM,
    CONF_SPEAKER,
    CONF_TRIGGER_ID,
)
import esphome.final_validate as fv

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@glmnet"]
CONF_RTTTL = "rtttl"
CONF_ON_FINISHED_PLAYBACK = "on_finished_playback"

rtttl_ns = cg.esphome_ns.namespace("rtttl")

Rtttl = rtttl_ns.class_("Rtttl", cg.Component)
PlayAction = rtttl_ns.class_("PlayAction", automation.Action)
StopAction = rtttl_ns.class_("StopAction", automation.Action)
FinishedPlaybackTrigger = rtttl_ns.class_(
    "FinishedPlaybackTrigger", automation.Trigger.template()
)
IsPlayingCondition = rtttl_ns.class_("IsPlayingCondition", automation.Condition)

MULTI_CONF = True

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(Rtttl),
            cv.Optional(CONF_OUTPUT): cv.use_id(FloatOutput),
            cv.Optional(CONF_SPEAKER): cv.use_id(Speaker),
            cv.Optional(CONF_GAIN, default="0.6"): cv.percentage,
            cv.Optional(CONF_ON_FINISHED_PLAYBACK): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        FinishedPlaybackTrigger
                    ),
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_OUTPUT, CONF_SPEAKER),
)


def validate_parent_output_config(value):
    platform = value.get(CONF_PLATFORM)
    PWM_GOOD = ["esp8266_pwm", "ledc"]
    PWM_BAD = [
        "ac_dimmer ",
        "esp32_dac",
        "slow_pwm",
        "mcp4725",
        "pca9685",
        "tlc59208f",
        "my9231",
        "sm16716",
    ]

    if platform in PWM_BAD:
        raise cv.Invalid(f"Component rtttl cannot use {platform} as output component")

    if platform not in PWM_GOOD:
        _LOGGER.warning(
            "Component rtttl is not known to work with the selected output type. "
            "Make sure this output supports custom frequency output method."
        )


FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_OUTPUT): fv.id_declaration_match_schema(
            validate_parent_output_config
        ),
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_OUTPUT in config:
        out = await cg.get_variable(config[CONF_OUTPUT])
        cg.add(var.set_output(out))
        cg.add_define("USE_OUTPUT")

    if CONF_SPEAKER in config:
        out = await cg.get_variable(config[CONF_SPEAKER])
        cg.add(var.set_speaker(out))

    cg.add(var.set_gain(config[CONF_GAIN]))

    for conf in config.get(CONF_ON_FINISHED_PLAYBACK, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


@automation.register_action(
    "rtttl.play",
    PlayAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(CONF_ID): cv.use_id(Rtttl),
            cv.Required(CONF_RTTTL): cv.templatable(cv.string),
        },
        key=CONF_RTTTL,
    ),
)
async def rtttl_play_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_RTTTL], args, cg.std_string)
    cg.add(var.set_value(template_))
    return var


@automation.register_action(
    "rtttl.stop",
    StopAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(Rtttl),
        }
    ),
)
async def rtttl_stop_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_condition(
    "rtttl.is_playing",
    IsPlayingCondition,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(Rtttl),
        }
    ),
)
async def rtttl_is_playing_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
