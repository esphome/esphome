import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components.output import FloatOutput
from esphome.components.speaker import Speaker
import esphome.config_validation as cv
from esphome.const import CONF_GAIN, CONF_ID, CONF_OUTPUT, CONF_PLATFORM, CONF_SPEAKER
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@glmnet", "@ximex"]
CONF_RTTTL = "rtttl"
# The player keeps the song length in 16 bits
MAX_SONG_LENGTH = 65535
CONF_ON_FINISHED_PLAYBACK = "on_finished_playback"

rtttl_ns = cg.esphome_ns.namespace("rtttl")

Rtttl = rtttl_ns.class_("Rtttl", cg.Component)

MULTI_CONF = True

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(Rtttl),
            cv.Optional(CONF_OUTPUT): cv.use_id(FloatOutput),
            cv.Optional(CONF_SPEAKER): cv.use_id(Speaker),
            cv.Optional(CONF_GAIN, default="0.6"): cv.percentage,
            cv.Optional(CONF_ON_FINISHED_PLAYBACK): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_OUTPUT, CONF_SPEAKER),
)


def validate_parent_output_config(value: ConfigType) -> None:
    platform = value.get(CONF_PLATFORM)
    PWM_GOOD = ["esp8266_pwm", "ledc"]
    PWM_BAD = [
        "ac_dimmer",
        "esp32_dac",
        "mcp4725",
        "my9231",
        "pca9685",
        "slow_pwm",
        "sm16716",
        "tlc59208f",
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


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_FINISHED_PLAYBACK, "add_on_finished_playback_callback"
    ),
)


async def to_code(config: ConfigType) -> None:
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

    if config.get(CONF_ON_FINISHED_PLAYBACK):
        cg.add_define("USE_RTTTL_FINISHED_PLAYBACK_CALLBACK")
        await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


def _static_song(config: ConfigType, value: str) -> str:
    # A constant song plays straight from static storage, flash on ESP8266, with no copy per play
    return f"esphome::rtttl::StaticSong{{{cg.FlashStringLiteral(value)}}}"


automation.register_apply_action(
    "rtttl.play",
    cv.maybe_simple_value(
        {
            cv.GenerateID(CONF_ID): cv.use_id(Rtttl),
            cv.Required(CONF_RTTTL): cv.templatable(
                cv.All(cv.string, cv.Length(max=MAX_SONG_LENGTH))
            ),
        },
        key=CONF_RTTTL,
    ),
    automation.ApplyField(CONF_RTTTL, "play", cg.std_string, const_fn=_static_song),
)

automation.register_apply_action(
    "rtttl.stop",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(Rtttl),
        }
    ),
    automation.ApplyCall("stop()"),
)


automation.register_apply_condition(
    "rtttl.is_playing",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(Rtttl),
        }
    ),
    "is_playing()",
)
