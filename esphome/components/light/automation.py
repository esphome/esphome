from collections.abc import Callable
import logging
from typing import Any, NamedTuple

from esphome import automation
import esphome.codegen as cg
from esphome.components.const.css_colors import CSS_COLORS
from esphome.config import path_context
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_BRIGHTNESS,
    CONF_BRIGHTNESS_LIMITS,
    CONF_COLD_WHITE,
    CONF_COLOR,
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
from esphome.core import CORE, ID, EsphomeError, Lambda
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

from .types import (
    COLOR_MODES,
    LIMIT_MODES,
    AddressableLightState,
    AddressableSet,
    ColorMode,
    DimRelativeAction,
    LightEffectCycleAction,
    LightState,
    ToggleAction,
)

_LOGGER = logging.getLogger(__name__)

CONF_INCLUDE_NONE = "include_none"

_STATE_ON_OFF = cv.one_of("ON", "OFF", upper=True)


@schema_extractor("one_of")
def validate_light_state(value: Any) -> Any:
    """Validate a light on/off state.

    Documented as 'ON'/'OFF', but accepts all boolean forms for backward compatibility.
    """
    if value == SCHEMA_EXTRACT:
        return ("ON", "OFF")
    try:
        return _STATE_ON_OFF(value) == "ON"
    except cv.Invalid:
        pass
    try:
        return cv.boolean(value)
    except cv.Invalid as err:
        raise cv.Invalid(
            f"Expected 'ON', 'OFF', or a boolean value, got {value!r}"
        ) from err


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
    has_transition_length = CONF_TRANSITION_LENGTH in config
    toggle_template_arg = cg.TemplateArguments(has_transition_length, *template_arg)
    var = cg.new_Pvariable(action_id, toggle_template_arg, paren)
    if has_transition_length:
        template_ = await cg.templatable(
            config[CONF_TRANSITION_LENGTH], args, cg.uint32
        )
        cg.add(var.set_transition_length(template_))
    return var


class LightStateField(NamedTuple):
    """One field of a light's state: the single source for the schemas and boot-time
    codegen that deal with it."""

    conf_key: str
    # Member of LightStateRTCState holding this field.
    member: str
    validator: Callable[[Any], Any]
    # The color mode providing this field, if it implies one.
    color_mode: MockObj | None = None
    templatable: bool = True


# In LightStateRTCState member order.
LIGHT_STATE_FIELDS: tuple[LightStateField, ...] = (
    LightStateField(CONF_STATE, "state", validate_light_state),
    LightStateField(
        CONF_COLOR_MODE,
        "color_mode",
        cv.enum(COLOR_MODES, upper=True, space="_"),
        templatable=False,
    ),
    LightStateField(CONF_BRIGHTNESS, "brightness", cv.percentage, ColorMode.BRIGHTNESS),
    LightStateField(
        CONF_COLOR_BRIGHTNESS, "color_brightness", cv.percentage, ColorMode.RGB
    ),
    LightStateField(CONF_RED, "red", cv.percentage, ColorMode.RGB),
    LightStateField(CONF_GREEN, "green", cv.percentage, ColorMode.RGB),
    LightStateField(CONF_BLUE, "blue", cv.percentage, ColorMode.RGB),
    LightStateField(CONF_WHITE, "white", cv.percentage, ColorMode.WHITE),
    LightStateField(
        CONF_COLOR_TEMPERATURE,
        "color_temp",
        cv.color_temperature,
        ColorMode.COLOR_TEMPERATURE,
    ),
    LightStateField(
        CONF_COLD_WHITE, "cold_white", cv.percentage, ColorMode.COLD_WARM_WHITE
    ),
    LightStateField(
        CONF_WARM_WHITE, "warm_white", cv.percentage, ColorMode.COLD_WARM_WHITE
    ),
)


@schema_extractor("one_of")
def validate_color(value: Any) -> str | int:
    """Validate a CSS color name or a 0xRRGGBB value."""
    if value == SCHEMA_EXTRACT:
        return ["CSS color name", "hex color value"]
    if isinstance(value, int) or (
        isinstance(value, str) and value.lower().startswith("0x")
    ):
        return cv.hex_int_range(0, 0xFFFFFF)(value)
    return cv.one_of(*CSS_COLORS, lower=True)(value)


COLOR_SCHEMA: dict[cv.Optional, Any] = {cv.Optional(CONF_COLOR): validate_color}


def color_to_rgb(config: ConfigType) -> ConfigType:
    """Replace a `color` CSS name or 0xRRGGBB value with red, green and blue values.

    The light scales its color so the brightest channel is at full level, so a dark
    color is given as a full-level color plus a color brightness.
    """
    if (color := config.pop(CONF_COLOR, None)) is None:
        return config
    if any(key in config for key in (CONF_RED, CONF_GREEN, CONF_BLUE)):
        raise cv.Invalid(
            f"'{CONF_COLOR}' cannot be used with '{CONF_RED}', '{CONF_GREEN}' or '{CONF_BLUE}'"
        )
    rgb = color if isinstance(color, int) else CSS_COLORS[color]
    channels = (rgb >> 16 & 0xFF, rgb >> 8 & 0xFF, rgb & 0xFF)
    peak = max(channels)
    if CONF_COLOR_BRIGHTNESS not in config:
        config[CONF_COLOR_BRIGHTNESS] = peak / 255
    elif peak < 0xFF:
        _LOGGER.warning(
            "'%s' overrides the brightness of color '%s'",
            CONF_COLOR_BRIGHTNESS,
            f"0x{color:06X}" if isinstance(color, int) else color,
        )
    for key, value in zip((CONF_RED, CONF_GREEN, CONF_BLUE), channels, strict=True):
        config[key] = value / peak if peak else 0.0
    return config


LIGHT_STATE_SCHEMA = cv.Schema(
    {
        cv.Optional(field.conf_key): (
            cv.templatable(field.validator) if field.templatable else field.validator
        )
        for field in LIGHT_STATE_FIELDS
    }
).extend(COLOR_SCHEMA)
LIGHT_STATE_SCHEMA.add_extra(color_to_rgb)

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


def _resolve_effect_index(config: ConfigType, original_name: str) -> int:
    """Resolve a static effect name to its 1-based index at codegen time.

    Effect index 0 means "None" (no effect). Effects are 1-indexed matching
    the C++ convention in LightState.
    """
    from . import available_effects_str, find_effect_index

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


def _effect_index(config: ConfigType, value: str) -> str:
    # Resolved at codegen time; the cast picks set_effect(uint32_t) over the optional overload.
    return f"static_cast<uint32_t>({_resolve_effect_index(config, value)})"


_LIGHT_CONTROL_FIELDS = (
    automation.ApplyField(CONF_COLOR_MODE, "set_color_mode", ColorMode),
    automation.ApplyField(CONF_STATE, "set_state", cg.bool_),
    automation.ApplyField(CONF_TRANSITION_LENGTH, "set_transition_length", cg.uint32),
    automation.ApplyField(CONF_FLASH_LENGTH, "set_flash_length", cg.uint32),
    automation.ApplyField(CONF_BRIGHTNESS, "set_brightness", cg.float_),
    automation.ApplyField(CONF_COLOR_BRIGHTNESS, "set_color_brightness", cg.float_),
    automation.ApplyField(CONF_RED, "set_red", cg.float_),
    automation.ApplyField(CONF_GREEN, "set_green", cg.float_),
    automation.ApplyField(CONF_BLUE, "set_blue", cg.float_),
    automation.ApplyField(CONF_WHITE, "set_white", cg.float_),
    automation.ApplyField(CONF_COLOR_TEMPERATURE, "set_color_temperature", cg.float_),
    automation.ApplyField(CONF_COLD_WHITE, "set_cold_white", cg.float_),
    automation.ApplyField(CONF_WARM_WHITE, "set_warm_white", cg.float_),
    automation.ApplyField(
        CONF_EFFECT, "set_effect", cg.std_string, const_fn=_effect_index
    ),
)

automation.register_apply_action(
    "light.turn_off",
    LIGHT_TURN_OFF_ACTION_SCHEMA,
    automation.ApplyField(CONF_STATE, "set_state", cg.bool_),
    automation.ApplyField(CONF_TRANSITION_LENGTH, "set_transition_length", cg.uint32),
    call="make_call",
)
automation.register_apply_action(
    "light.turn_on",
    LIGHT_TURN_ON_ACTION_SCHEMA,
    *_LIGHT_CONTROL_FIELDS,
    call="make_call",
)
automation.register_apply_action(
    "light.control",
    LIGHT_CONTROL_ACTION_SCHEMA,
    *_LIGHT_CONTROL_FIELDS,
    call="make_call",
)


def _record_effect_cycle_ref(config: ConfigType) -> ConfigType:
    """Record a cycle-action reference for later validation against the target light."""
    from . import EffectCycleRef, _get_data

    _get_data().effect_cycle_refs.append(
        EffectCycleRef(
            light_id=config[CONF_ID],
            component_path=path_context.get(),
        )
    )
    return config


LIGHT_EFFECT_CYCLE_ACTION_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(LightState),
        cv.Optional(CONF_INCLUDE_NONE, default=False): cv.boolean,
    }
)
LIGHT_EFFECT_CYCLE_ACTION_BASE_SCHEMA.add_extra(_record_effect_cycle_ref)

LIGHT_EFFECT_CYCLE_ACTION_SCHEMA = automation.maybe_simple_id(
    LIGHT_EFFECT_CYCLE_ACTION_BASE_SCHEMA
)


@automation.register_action(
    "light.effect.next",
    LightEffectCycleAction,
    LIGHT_EFFECT_CYCLE_ACTION_SCHEMA,
    synchronous=True,
)
async def light_effect_next_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _light_effect_cycle_to_code(config, action_id, template_arg, True)


@automation.register_action(
    "light.effect.previous",
    LightEffectCycleAction,
    LIGHT_EFFECT_CYCLE_ACTION_SCHEMA,
    synchronous=True,
)
async def light_effect_previous_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _light_effect_cycle_to_code(config, action_id, template_arg, False)


async def _light_effect_cycle_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    forward: bool,
) -> MockObj:
    paren = await cg.get_variable(config[CONF_ID])
    cycle_template_arg = cg.TemplateArguments(forward, *template_arg)
    var = cg.new_Pvariable(action_id, cycle_template_arg, paren)
    cg.add(var.set_include_none(config[CONF_INCLUDE_NONE]))
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
    has_transition_length = CONF_TRANSITION_LENGTH in config
    dim_template_arg = cg.TemplateArguments(has_transition_length, *template_arg)
    var = cg.new_Pvariable(action_id, dim_template_arg, paren)
    templ = await cg.templatable(config[CONF_RELATIVE_BRIGHTNESS], args, cg.float_)
    cg.add(var.set_relative_brightness(templ))
    if has_transition_length:
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
).extend(COLOR_SCHEMA)
LIGHT_ADDRESSABLE_SET_ACTION_SCHEMA.add_extra(color_to_rgb)


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


LIGHT_CONDITION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(LightState),
    }
)

automation.register_apply_condition(
    "light.is_on", LIGHT_CONDITION_SCHEMA, "current_values.is_on()"
)
automation.register_apply_condition(
    "light.is_off", LIGHT_CONDITION_SCHEMA, "current_values.is_on() == false"
)
