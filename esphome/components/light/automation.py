from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.config import path_context
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
    CONF_EFFECTS,
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
from esphome.core import CORE, EsphomeError, Lambda
from esphome.cpp_generator import LambdaExpression
from esphome.types import ConfigType, SafeExpType

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
    synchronous=True,
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


def _record_effect_ref(config: ConfigType) -> ConfigType:
    """Record a static effect name reference for later cross-component validation."""
    if CONF_EFFECT not in config:
        return config
    effect = config[CONF_EFFECT]
    if isinstance(effect, Lambda):
        return config  # Lambda effects resolved at runtime
    if effect.lower() == "none":
        return config  # "None" is always valid

    from . import EffectRef, _get_data

    _get_data().effect_refs.append(
        EffectRef(
            light_id=config[CONF_ID],
            effect_name=effect,
            component_path=path_context.get(),
        )
    )
    return config


LIGHT_CONTROL_ACTION_SCHEMA.add_extra(_record_effect_ref)

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


async def _as_lambda(
    value: Any,
    args: list[tuple[SafeExpType, str]],
    output_type: SafeExpType,
) -> LambdaExpression:
    """Return a stateless lambda expression for a templatable value.

    If value is already a lambda, process it normally. Otherwise wrap
    the constant in a ``[](...) -> T { return <value>; }`` expression
    so that LightControlAction can store every field as a plain
    function pointer.
    """
    if cg.is_template(value):
        return await cg.process_lambda(value, args, return_type=output_type)
    return LambdaExpression(
        f"return {cg.safe_exp(value)};",
        args,
        capture="",
        return_type=output_type,
    )


def _resolve_effect_index(config: ConfigType) -> int:
    """Resolve a static effect name to its 1-based index at codegen time.

    Effect index 0 means "None" (no effect). Effects are 1-indexed matching
    the C++ convention in LightState.
    """
    from . import available_effects_str, find_effect_index

    original_name = config[CONF_EFFECT]
    if original_name.lower() == "none":
        return 0
    light_id = config[CONF_ID]
    light_path = CORE.config.get_path_for_id(light_id)[:-1]
    light_config = CORE.config.get_config_for_path(light_path)
    effects = light_config.get(CONF_EFFECTS, [])
    index = find_effect_index(effects, original_name)
    if index is not None:
        return index
    # Should never reach here — effect names are validated during config
    # validation in FINAL_VALIDATE_SCHEMA. This is a safety net.
    raise EsphomeError(
        f"Effect '{original_name}' not found for light '{light_id}'. "
        f"Available effects: {available_effects_str(effects)}"
    )


@automation.register_action(
    "light.turn_off", LightControlAction, LIGHT_TURN_OFF_ACTION_SCHEMA, synchronous=True
)
@automation.register_action(
    "light.turn_on", LightControlAction, LIGHT_TURN_ON_ACTION_SCHEMA, synchronous=True
)
@automation.register_action(
    "light.control", LightControlAction, LIGHT_CONTROL_ACTION_SCHEMA, synchronous=True
)
async def light_control_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    # (config_key, setter_name, c++ type)
    FIELDS = (
        (CONF_COLOR_MODE, "set_color_mode", ColorMode),
        (CONF_STATE, "set_state", bool),
        (CONF_TRANSITION_LENGTH, "set_transition_length", cg.uint32),
        (CONF_FLASH_LENGTH, "set_flash_length", cg.uint32),
        (CONF_BRIGHTNESS, "set_brightness", float),
        (CONF_COLOR_BRIGHTNESS, "set_color_brightness", float),
        (CONF_RED, "set_red", float),
        (CONF_GREEN, "set_green", float),
        (CONF_BLUE, "set_blue", float),
        (CONF_WHITE, "set_white", float),
        (CONF_COLOR_TEMPERATURE, "set_color_temperature", float),
        (CONF_COLD_WHITE, "set_cold_white", float),
        (CONF_WARM_WHITE, "set_warm_white", float),
    )
    for conf_key, setter, type_ in FIELDS:
        if conf_key in config:
            cg.add(
                getattr(var, setter)(await _as_lambda(config[conf_key], args, type_))
            )

    if CONF_EFFECT in config:
        if isinstance(config[CONF_EFFECT], Lambda):
            # Lambda returns a string — wrap in a C++ lambda that resolves
            # the effect name to its uint32_t index at runtime
            inner_lambda = await cg.process_lambda(
                config[CONF_EFFECT], args, return_type=cg.std_string
            )
            fwd_args = ", ".join(n for _, n in args)
            # capture="" is correct: paren is a global variable name
            # string-interpolated into the body at codegen time, not a
            # C++ runtime capture.
            wrapper = LambdaExpression(
                f"auto __effect_s = ({inner_lambda})({fwd_args});\n"
                f"return {paren}->get_effect_index("
                f"__effect_s.c_str(), __effect_s.size());",
                args,
                capture="",
                return_type=cg.uint32,
            )
            cg.add(var.set_effect(wrapper))
        else:
            # Static string — resolve effect name to index at codegen time
            cg.add(
                var.set_effect(
                    await _as_lambda(_resolve_effect_index(config), args, cg.uint32)
                )
            )
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
    "light.dim_relative",
    DimRelativeAction,
    LIGHT_DIM_RELATIVE_ACTION_SCHEMA,
    synchronous=True,
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
    "light.addressable_set",
    AddressableSet,
    LIGHT_ADDRESSABLE_SET_ACTION_SCHEMA,
    synchronous=True,
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
