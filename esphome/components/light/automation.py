from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_BRIGHTNESS,
    CONF_BRIGHTNESS_LIMITS,
    CONF_COLD_WHITE,
    CONF_COLOR_BRIGHTNESS,
    CONF_COLOR_MODE,
    CONF_COLOR_TEMPERATURE,
    CONF_EFFECT,
    CONF_FLASH_LENGTH,
    CONF_GREEN,
    CONF_ID,
    CONF_LIMIT_MODE,
    CONF_MAX_BRIGHTNESS,
    CONF_MIN_BRIGHTNESS,
    CONF_RANGE_FROM,
    CONF_RANGE_TO,
    CONF_RED,
    CONF_STATE,
    CONF_TRANSITION_LENGTH,
    CONF_WARM_WHITE,
    CONF_WHITE,
)

from .types import (
    COLOR_MODES,
    LIMIT_MODES,
    AddressableLightState,
    AddressableSet,
    ColorMode,
    DimRelativeAction,
    LightControlAction,
    LightIsOffCondition,
    LightIsOnCondition,
    LightState,
    ToggleAction,
)


@automation.register_action(
    "light.toggle",
    ToggleAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(LightState),
            cv.Optional(CONF_TRANSITION_LENGTH): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
)
async def light_toggle_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_TRANSITION_LENGTH in config:
        template_ = await cg.templatable(
            config[CONF_TRANSITION_LENGTH], args, cg.uint32
        )
        cg.add(var.set_transition_length(template_))
    return var


LIGHT_STATE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_COLOR_MODE): cv.enum(COLOR_MODES, upper=True, space="_"),
        cv.Optional(CONF_STATE): cv.templatable(cv.boolean),
        cv.Optional(CONF_BRIGHTNESS): cv.templatable(cv.percentage),
        cv.Optional(CONF_COLOR_BRIGHTNESS): cv.templatable(cv.percentage),
        cv.Optional(CONF_RED): cv.templatable(cv.percentage),
        cv.Optional(CONF_GREEN): cv.templatable(cv.percentage),
        cv.Optional(CONF_BLUE): cv.templatable(cv.percentage),
        cv.Optional(CONF_WHITE): cv.templatable(cv.percentage),
        cv.Optional(CONF_COLOR_TEMPERATURE): cv.templatable(cv.color_temperature),
        cv.Optional(CONF_COLD_WHITE): cv.templatable(cv.percentage),
        cv.Optional(CONF_WARM_WHITE): cv.templatable(cv.percentage),
    }
)

LIGHT_CONTROL_ACTION_SCHEMA = LIGHT_STATE_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.use_id(LightState),
        cv.Exclusive(CONF_TRANSITION_LENGTH, "transformer"): cv.templatable(
            cv.positive_time_period_milliseconds
        ),
        cv.Exclusive(CONF_FLASH_LENGTH, "transformer"): cv.templatable(
            cv.positive_time_period_milliseconds
        ),
        cv.Exclusive(CONF_EFFECT, "transformer"): cv.templatable(cv.string),
    }
)

LIGHT_TURN_OFF_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(LightState),
        cv.Optional(CONF_TRANSITION_LENGTH): cv.templatable(
            cv.positive_time_period_milliseconds
        ),
        cv.Optional(CONF_STATE, default=False): False,
    }
)
LIGHT_TURN_ON_ACTION_SCHEMA = automation.maybe_simple_id(
    LIGHT_CONTROL_ACTION_SCHEMA.extend(
        {
            cv.Optional(CONF_STATE, default=True): True,
        }
    )
)


@automation.register_action(
    "light.turn_off", LightControlAction, LIGHT_TURN_OFF_ACTION_SCHEMA
)
@automation.register_action(
    "light.turn_on", LightControlAction, LIGHT_TURN_ON_ACTION_SCHEMA
)
@automation.register_action(
    "light.control", LightControlAction, LIGHT_CONTROL_ACTION_SCHEMA
)
async def light_control_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_COLOR_MODE in config:
        template_ = await cg.templatable(config[CONF_COLOR_MODE], args, ColorMode)
        cg.add(var.set_color_mode(template_))
    if CONF_STATE in config:
        template_ = await cg.templatable(config[CONF_STATE], args, bool)
        cg.add(var.set_state(template_))
    if CONF_TRANSITION_LENGTH in config:
        template_ = await cg.templatable(
            config[CONF_TRANSITION_LENGTH], args, cg.uint32
        )
        cg.add(var.set_transition_length(template_))
    if CONF_FLASH_LENGTH in config:
        template_ = await cg.templatable(config[CONF_FLASH_LENGTH], args, cg.uint32)
        cg.add(var.set_flash_length(template_))
    if CONF_BRIGHTNESS in config:
        template_ = await cg.templatable(config[CONF_BRIGHTNESS], args, float)
        cg.add(var.set_brightness(template_))
    if CONF_COLOR_BRIGHTNESS in config:
        template_ = await cg.templatable(config[CONF_COLOR_BRIGHTNESS], args, float)
        cg.add(var.set_color_brightness(template_))
    if CONF_RED in config:
        template_ = await cg.templatable(config[CONF_RED], args, float)
        cg.add(var.set_red(template_))
    if CONF_GREEN in config:
        template_ = await cg.templatable(config[CONF_GREEN], args, float)
        cg.add(var.set_green(template_))
    if CONF_BLUE in config:
        template_ = await cg.templatable(config[CONF_BLUE], args, float)
        cg.add(var.set_blue(template_))
    if CONF_WHITE in config:
        template_ = await cg.templatable(config[CONF_WHITE], args, float)
        cg.add(var.set_white(template_))
    if CONF_COLOR_TEMPERATURE in config:
        template_ = await cg.templatable(config[CONF_COLOR_TEMPERATURE], args, float)
        cg.add(var.set_color_temperature(template_))
    if CONF_COLD_WHITE in config:
        template_ = await cg.templatable(config[CONF_COLD_WHITE], args, float)
        cg.add(var.set_cold_white(template_))
    if CONF_WARM_WHITE in config:
        template_ = await cg.templatable(config[CONF_WARM_WHITE], args, float)
        cg.add(var.set_warm_white(template_))
    if CONF_EFFECT in config:
        template_ = await cg.templatable(config[CONF_EFFECT], args, cg.std_string)
        cg.add(var.set_effect(template_))
    return var


CONF_RELATIVE_BRIGHTNESS = "relative_brightness"
LIGHT_DIM_RELATIVE_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(LightState),
        cv.Required(CONF_RELATIVE_BRIGHTNESS): cv.templatable(
            cv.possibly_negative_percentage
        ),
        cv.Optional(CONF_TRANSITION_LENGTH): cv.templatable(
            cv.positive_time_period_milliseconds
        ),
        cv.Optional(CONF_BRIGHTNESS_LIMITS): cv.Schema(
            {
                cv.Optional(CONF_MIN_BRIGHTNESS, default="0%"): cv.percentage,
                cv.Optional(CONF_MAX_BRIGHTNESS, default="100%"): cv.percentage,
                cv.Optional(CONF_LIMIT_MODE, default="CLAMP"): cv.enum(
                    LIMIT_MODES, upper=True, space="_"
                ),
            }
        ),
    }
)


@automation.register_action(
    "light.dim_relative", DimRelativeAction, LIGHT_DIM_RELATIVE_ACTION_SCHEMA
)
async def light_dim_relative_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    templ = await cg.templatable(config[CONF_RELATIVE_BRIGHTNESS], args, float)
    cg.add(var.set_relative_brightness(templ))
    if CONF_TRANSITION_LENGTH in config:
        templ = await cg.templatable(config[CONF_TRANSITION_LENGTH], args, cg.uint32)
        cg.add(var.set_transition_length(templ))
    if conf := config.get(CONF_BRIGHTNESS_LIMITS):
        cg.add(
            var.set_min_max_brightness(
                conf[CONF_MIN_BRIGHTNESS], conf[CONF_MAX_BRIGHTNESS]
            )
        )
        cg.add(var.set_limit_mode(conf[CONF_LIMIT_MODE]))
    return var


LIGHT_ADDRESSABLE_SET_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(AddressableLightState),
        cv.Optional(CONF_RANGE_FROM): cv.templatable(cv.positive_int),
        cv.Optional(CONF_RANGE_TO): cv.templatable(cv.positive_int),
        cv.Optional(CONF_COLOR_BRIGHTNESS): cv.templatable(cv.percentage),
        cv.Optional(CONF_RED): cv.templatable(cv.percentage),
        cv.Optional(CONF_GREEN): cv.templatable(cv.percentage),
        cv.Optional(CONF_BLUE): cv.templatable(cv.percentage),
        cv.Optional(CONF_WHITE): cv.templatable(cv.percentage),
    }
)


@automation.register_action(
    "light.addressable_set", AddressableSet, LIGHT_ADDRESSABLE_SET_ACTION_SCHEMA
)
async def light_addressable_set_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_RANGE_FROM in config:
        templ = await cg.templatable(config[CONF_RANGE_FROM], args, cg.int32)
        cg.add(var.set_range_from(templ))
    if CONF_RANGE_TO in config:
        templ = await cg.templatable(config[CONF_RANGE_TO], args, cg.int32)
        cg.add(var.set_range_to(templ))

    if CONF_COLOR_BRIGHTNESS in config:
        templ = await cg.templatable(config[CONF_COLOR_BRIGHTNESS], args, cg.float_)
        cg.add(var.set_color_brightness(templ))
    if CONF_RED in config:
        templ = await cg.templatable(config[CONF_RED], args, cg.float_)
        cg.add(var.set_red(templ))
    if CONF_GREEN in config:
        templ = await cg.templatable(config[CONF_GREEN], args, cg.float_)
        cg.add(var.set_green(templ))
    if CONF_BLUE in config:
        templ = await cg.templatable(config[CONF_BLUE], args, cg.float_)
        cg.add(var.set_blue(templ))
    if CONF_WHITE in config:
        templ = await cg.templatable(config[CONF_WHITE], args, cg.float_)
        cg.add(var.set_white(templ))
    return var


@automation.register_condition(
    "light.is_on",
    LightIsOnCondition,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(LightState),
        }
    ),
)
@automation.register_condition(
    "light.is_off",
    LightIsOffCondition,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(LightState),
        }
    ),
)
async def light_is_on_off_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
