from collections.abc import Callable
from dataclasses import dataclass, field
import enum
import logging

import esphome.automation as auto
import esphome.codegen as cg
from esphome.components import mqtt, power_supply, web_server
from esphome.components.const import CONF_CHANNEL_COLORS, CONF_IS_WRGB
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_COLOR_CORRECT,
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_EFFECTS,
    CONF_ENTITY_CATEGORY,
    CONF_FLASH_TRANSITION_LENGTH,
    CONF_GAMMA_CORRECT,
    CONF_ICON,
    CONF_ID,
    CONF_INITIAL_STATE,
    CONF_IS_RGBW,
    CONF_MQTT_ID,
    CONF_NAME,
    CONF_ON_STATE,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_OUTPUT_ID,
    CONF_POWER_SUPPLY,
    CONF_RESTORE_MODE,
    CONF_RESTORE_STATE,
    CONF_RGB_ORDER,
    CONF_TRIGGER_ID,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
    CONF_WEB_SERVER,
)
from esphome.core import CORE, ID, CoroPriority, HexInt, coroutine_with_priority
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    queue_entity_register,
    setup_entity,
)
from esphome.cpp_generator import MockObjClass
import esphome.final_validate as fv
from esphome.types import ConfigType

from .automation import LIGHT_STATE_SCHEMA
from .effects import (
    ADDRESSABLE_EFFECTS,
    BINARY_EFFECTS,
    EFFECTS_REGISTRY,
    MONOCHROMATIC_EFFECTS,
    RGB_EFFECTS,
    validate_effects,
)
from .restore_state import (
    LEGACY_RESTORE_MODES,
    RESTORE_STATE_NONE,
    RESTORE_STATE_SCHEMA,
    _build_state_lambda,
    _initial_state_overridden_by_legacy_mode,
    _initial_state_statements,
    _legacy_cold_boot_statements,
    _legacy_restore_statements,
    _restore_state_statements,
)
from .types import (  # noqa: F401
    AddressableLight,
    AddressableLightState,
    ChannelColors,
    ColorMode,
    LightOutput,
    LightState,
    LightStateRTCState,
    LightStateTrigger,
    LightTurnOffTrigger,
    LightTurnOnTrigger,
    light_ns,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True

DOMAIN = "light"


@dataclass
class EffectRef:
    """A pending effect name reference from a light action to validate."""

    light_id: ID
    effect_name: str
    component_path: list[str | int]  # path_context when the action was validated


@dataclass
class EffectCycleRef:
    """A pending light.effect.next/previous action to validate.

    Records that the referenced light needs at least one effect configured.
    """

    light_id: ID
    component_path: list[str | int]


@dataclass
class LightData:
    gamma_tables: dict = field(default_factory=dict)  # gamma_value -> fwd_arr
    effect_refs: list[EffectRef] = field(default_factory=list)
    effect_cycle_refs: list[EffectCycleRef] = field(default_factory=list)


def _get_data() -> LightData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = LightData()
    return CORE.data[DOMAIN]


def generate_gamma_table(gamma_correct: float) -> list[HexInt]:
    """Generate a 256-entry uint16 gamma lookup table.

    For gamma > 0, non-zero indices are clamped to a minimum of 1 to preserve
    the invariant that non-zero input always produces non-zero output. Without
    this, small brightness values (e.g. 1%) get quantized to exactly 0.0,
    which breaks zero_means_zero logic in FloatOutput.
    """
    if gamma_correct > 0:
        return [
            HexInt(
                max(1, min(65535, int(round((i / 255.0) ** gamma_correct * 65535))))
                if i > 0
                else HexInt(0)
            )
            for i in range(256)
        ]
    return [HexInt(int(round(i / 255.0 * 65535))) for i in range(256)]


def _get_or_create_gamma_table(gamma_correct):
    data = _get_data()
    if gamma_correct in data.gamma_tables:
        return data.gamma_tables[gamma_correct]

    forward = generate_gamma_table(gamma_correct)

    gamma_str = f"{gamma_correct}".replace(".", "_")
    fwd_id = ID(f"gamma_{gamma_str}_fwd", is_declaration=True, type=cg.uint16)
    fwd_arr = cg.progmem_array(fwd_id, forward)
    data.gamma_tables[gamma_correct] = fwd_arr
    return fwd_arr


def find_effect_index(effects: list, effect_name: str) -> int | None:
    """Find the 1-based index of an effect by name (case-insensitive).

    Returns the 1-based index if found, or None if not found.
    """
    effect_name_lower = effect_name.lower()
    for i, effect_conf in enumerate(effects):
        key = next(iter(effect_conf))
        if effect_conf[key][CONF_NAME].lower() == effect_name_lower:
            return i + 1
    return None


def available_effects_str(effects: list) -> str:
    """Return a comma-separated string of available effect names."""
    available = [
        effect_conf[next(iter(effect_conf))][CONF_NAME] for effect_conf in effects
    ]
    return ", ".join(f"'{name}'" for name in available) if available else "none"


# Accepted values of the deprecated `rgb_order` key.
RGB_ORDERS = ("RGB", "RBG", "GRB", "GBR", "BGR", "BRG")

_RGB_CHANNELS = frozenset("RGB")
_RGBW_CHANNELS = frozenset("RGBW")


def validate_channel_colors(value: str) -> str:
    """Validate the channel order of an addressable strip, e.g. "GRB" or "WRGB"."""
    value = cv.string_strict(value).upper()
    channels = frozenset(value)
    if len(channels) != len(value) or channels not in (_RGB_CHANNELS, _RGBW_CHANNELS):
        raise cv.Invalid(
            f"'{value}' is not a valid channel order. List each of R, G and B exactly "
            "once, optionally with a single W, in the order the strip expects them "
            "(for example GRB, GRBW or WRGB)"
        )
    return value


def channel_colors_struct(value: str) -> cg.StructInitializer:
    """Build the C++ `light::ChannelColors` for a validated channel order string."""
    return cg.StructInitializer(
        ChannelColors,
        ("r", value.index("R")),
        ("g", value.index("G")),
        ("b", value.index("B")),
        (
            "w",
            value.index("W")
            if "W" in value
            else cg.RawExpression(f"{ChannelColors}::NO_WHITE"),
        ),
    )


def _quote_and_join(keys: list[str]) -> str:
    """Quote each key and join them into a readable list, e.g. "'a', 'b' and 'c'"."""
    quoted = [f"'{key}'" for key in keys]
    if len(quoted) == 1:
        return quoted[0]
    return f"{', '.join(quoted[:-1])} and {quoted[-1]}"


def migrate_channel_colors(
    *, removed_in: str, component: str
) -> Callable[[ConfigType], ConfigType]:
    """Fold the deprecated `rgb_order`, `is_rgbw` and `is_wrgb` keys into `channel_colors`.

    This also enforces that `channel_colors` is set, which the schema cannot do on its
    own while the deprecated keys are still accepted. After this runs, `to_code` only
    ever sees `channel_colors`.
    """

    def validator(config: ConfigType) -> ConfigType:
        config = config.copy()
        deprecated = [
            key for key in (CONF_RGB_ORDER, CONF_IS_RGBW, CONF_IS_WRGB) if key in config
        ]
        if CONF_CHANNEL_COLORS in config:
            if deprecated:
                raise cv.Invalid(
                    f"'{CONF_CHANNEL_COLORS}' cannot be combined with "
                    f"{_quote_and_join(deprecated)}"
                )
            return config
        if CONF_RGB_ORDER not in config:
            raise cv.Invalid(
                f"'{CONF_CHANNEL_COLORS}' is required", path=[CONF_CHANNEL_COLORS]
            )
        rgb_order = config.pop(CONF_RGB_ORDER)
        is_rgbw = config.pop(CONF_IS_RGBW, False)
        is_wrgb = config.pop(CONF_IS_WRGB, False)
        if is_rgbw and is_wrgb:
            raise cv.Invalid(
                f"'{CONF_IS_RGBW}' and '{CONF_IS_WRGB}' cannot both be enabled"
            )
        if is_wrgb:
            channel_colors = f"W{rgb_order}"
        elif is_rgbw:
            channel_colors = f"{rgb_order}W"
        else:
            channel_colors = rgb_order
        _LOGGER.warning(
            "[%s] %s %s deprecated, use '%s: %s'. Will be removed in %s",
            component,
            _quote_and_join(deprecated),
            "are" if len(deprecated) > 1 else "is",
            CONF_CHANNEL_COLORS,
            channel_colors,
            removed_in,
        )
        config[CONF_CHANNEL_COLORS] = channel_colors
        return config

    return validator


def _final_validate(config: ConfigType) -> None:
    """Validate all recorded effect name references against their target lights.

    This runs once per light platform instance. If no light platform is configured,
    this never runs — but the ID validator will catch the missing light ID separately.
    """
    data = _get_data()
    if not data.effect_refs and not data.effect_cycle_refs:
        return

    # Drain the lists so we only validate once even though
    # FINAL_VALIDATE_SCHEMA runs for each light platform instance.
    refs = data.effect_refs
    data.effect_refs = []
    cycle_refs = data.effect_cycle_refs
    data.effect_cycle_refs = []

    fconf = fv.full_config.get()

    for ref in refs:
        try:
            light_path = fconf.get_path_for_id(ref.light_id)[:-1]
            light_config = fconf.get_config_for_path(light_path)
        except KeyError:
            # Light ID not found — ID validation will have already reported this
            continue

        effects = light_config.get(CONF_EFFECTS, [])

        if find_effect_index(effects, ref.effect_name) is None:
            raise cv.FinalExternalInvalid(
                f"Effect '{ref.effect_name}' not found for light "
                f"'{ref.light_id}'. "
                f"Available effects: {available_effects_str(effects)}",
                path=[cv.ROOT_CONFIG_PATH] + ref.component_path,
            )

    for ref in cycle_refs:
        try:
            light_path = fconf.get_path_for_id(ref.light_id)[:-1]
            light_config = fconf.get_config_for_path(light_path)
        except KeyError:
            continue

        if not light_config.get(CONF_EFFECTS):
            raise cv.FinalExternalInvalid(
                f"Light '{ref.light_id}' has no effects configured, but a "
                f"'light.effect.next' or 'light.effect.previous' action "
                f"references it. Add at least one effect to the light.",
                path=[cv.ROOT_CONFIG_PATH] + ref.component_path,
            )


FINAL_VALIDATE_SCHEMA = _final_validate


LIGHT_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(LightState),
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt.MQTTJSONLightComponent
            ),
            cv.Exclusive(CONF_RESTORE_MODE, "restore"): cv.one_of(
                *LEGACY_RESTORE_MODES, upper=True, space="_"
            ),
            cv.Exclusive(CONF_RESTORE_STATE, "restore"): RESTORE_STATE_SCHEMA,
            cv.Optional(CONF_ON_TURN_ON): auto.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LightTurnOnTrigger),
                }
            ),
            cv.Optional(CONF_ON_TURN_OFF): auto.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LightTurnOffTrigger),
                }
            ),
            cv.Optional(CONF_ON_STATE): auto.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LightStateTrigger),
                }
            ),
            cv.Optional(CONF_INITIAL_STATE): LIGHT_STATE_SCHEMA,
        }
    )
)

LIGHT_SCHEMA.add_extra(entity_duplicate_validator("light"))

BINARY_LIGHT_SCHEMA = LIGHT_SCHEMA.extend(
    {
        cv.Optional(CONF_EFFECTS): validate_effects(BINARY_EFFECTS),
    }
)

BRIGHTNESS_ONLY_LIGHT_SCHEMA = LIGHT_SCHEMA.extend(
    {
        cv.Optional(CONF_GAMMA_CORRECT, default=2.8): cv.positive_float,
        cv.Optional(
            CONF_DEFAULT_TRANSITION_LENGTH, default="1s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_FLASH_TRANSITION_LENGTH, default="0s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_EFFECTS): validate_effects(MONOCHROMATIC_EFFECTS),
    }
)

RGB_LIGHT_SCHEMA = BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.Optional(CONF_EFFECTS): validate_effects(RGB_EFFECTS),
    }
)

ADDRESSABLE_LIGHT_SCHEMA = RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AddressableLightState),
        cv.Optional(CONF_EFFECTS): validate_effects(ADDRESSABLE_EFFECTS),
        cv.Optional(CONF_COLOR_CORRECT): cv.All(
            [cv.percentage], cv.Length(min=3, max=4)
        ),
        cv.Optional(CONF_POWER_SUPPLY): cv.use_id(power_supply.PowerSupply),
    }
)


class LightType(enum.IntEnum):
    """Light type enum."""

    BINARY = 0
    BRIGHTNESS_ONLY = 1
    RGB = 2
    ADDRESSABLE = 3


def _apply_default_restore_mode(
    default_restore_mode: str,
) -> Callable[[ConfigType], ConfigType]:
    # cv.Exclusive (unlike cv.Optional) has no `default` parameter. Applying the
    # platform's default as a post-validation step (via add_extra, below) runs after
    # LIGHT_SCHEMA's cv.Exclusive("restore") marker has already checked the raw input,
    # so filling in restore_mode: here can never trip that check -- and only happens
    # when the user gave neither restore_mode nor restore_state, so a user-provided
    # restore_state is never silently overridden.
    def validator(config: ConfigType) -> ConfigType:
        if CONF_RESTORE_MODE not in config and CONF_RESTORE_STATE not in config:
            config[CONF_RESTORE_MODE] = cv.one_of(
                *LEGACY_RESTORE_MODES, upper=True, space="_"
            )(default_restore_mode)
        return config

    return validator


def light_schema(
    class_: MockObjClass,
    type_: LightType,
    *,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
    default_restore_mode: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(class_),
    }

    for key, default, validator in [
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_ICON, icon, cv.icon),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    if type_ == LightType.BINARY:
        base_schema = BINARY_LIGHT_SCHEMA
    elif type_ == LightType.BRIGHTNESS_ONLY:
        base_schema = BRIGHTNESS_ONLY_LIGHT_SCHEMA
    elif type_ == LightType.RGB:
        base_schema = RGB_LIGHT_SCHEMA
    elif type_ == LightType.ADDRESSABLE:
        base_schema = ADDRESSABLE_LIGHT_SCHEMA
    else:
        raise ValueError(f"Invalid light type: {type_}")

    result = base_schema.extend(schema)
    if default_restore_mode is not cv.UNDEFINED:
        result.add_extra(_apply_default_restore_mode(default_restore_mode))
    return result


def validate_color_temperature_channels(value):
    if (
        CONF_COLD_WHITE_COLOR_TEMPERATURE in value
        and CONF_WARM_WHITE_COLOR_TEMPERATURE in value
        and value[CONF_COLD_WHITE_COLOR_TEMPERATURE]
        >= value[CONF_WARM_WHITE_COLOR_TEMPERATURE]
    ):
        raise cv.Invalid(
            "Color temperature of the cold white channel must be colder than that of the warm white channel.",
            path=[CONF_COLD_WHITE_COLOR_TEMPERATURE],
        )
    return value


@setup_entity("light")
async def setup_light_core_(light_var, config, output_var):
    # All 8 legacy restore_mode values, and the restore_state key, are just different
    # ways to build the same state callback and save_enabled flag that LightState's
    # runtime actually understands.
    initial_state_config = config.get(CONF_INITIAL_STATE)
    initial_statements = await _initial_state_statements(initial_state_config)

    restore_mode = config.get(CONF_RESTORE_MODE)
    restore_state_config = config.get(CONF_RESTORE_STATE)
    if restore_state_config == RESTORE_STATE_NONE:
        # restore_state: none is explicit shorthand for "no restoring at all" --
        # normalize it to the same thing as omitting restore_state: entirely.
        restore_state_config = None

    if restore_mode is not None:
        legacy = LEGACY_RESTORE_MODES[restore_mode]
        if _initial_state_overridden_by_legacy_mode(legacy, initial_state_config):
            _LOGGER.warning(
                "[%s] 'initial_state: state' is ignored because 'restore_mode: %s' "
                "always sets the light %s at boot; use 'restore_state:' instead for "
                "per-field control",
                config.get(CONF_NAME) or config[CONF_ID],
                restore_mode,
                "ON" if legacy.cold_boot_state else "OFF",
            )
        initial_statements.extend(
            _legacy_cold_boot_statements(legacy, initial_state_config)
        )
        restore_statements = _legacy_restore_statements(legacy)
        save_enabled = legacy.save_enabled
    elif restore_state_config is not None:
        restore_statements = _restore_state_statements(
            restore_state_config, initial_state_config
        )
        save_enabled = True
    else:
        # Neither key configured: no persistence, and -- unlike every explicit
        # restore_mode: value -- no cold-boot forcing either. The light simply comes
        # up as initial_state: (or LightStateRTCState's own defaults) says, same as
        # any other boot.
        restore_statements = []
        save_enabled = False

    if (
        lamb := await _build_state_lambda(
            initial_statements, restore_statements, save_enabled
        )
    ) is not None:
        cg.add(light_var.set_state_callback(lamb))
    if save_enabled:  # matches LightState::save_enabled_'s own default of false
        cg.add(light_var.set_save_enabled(save_enabled))

    if (
        default_transition_length := config.get(CONF_DEFAULT_TRANSITION_LENGTH)
    ) is not None:
        cg.add(light_var.set_default_transition_length(default_transition_length))
    if (
        flash_transition_length := config.get(CONF_FLASH_TRANSITION_LENGTH)
    ) is not None:
        cg.add(light_var.set_flash_transition_length(flash_transition_length))
    if (gamma_correct := config.get(CONF_GAMMA_CORRECT)) is not None:
        cg.add(light_var.set_gamma_correct(gamma_correct))
        fwd_arr = _get_or_create_gamma_table(gamma_correct)
        cg.add(light_var.set_gamma_table(fwd_arr))
        cg.add_define("USE_LIGHT_GAMMA_LUT")
    effects = await cg.build_registry_list(
        EFFECTS_REGISTRY, config.get(CONF_EFFECTS, [])
    )
    cg.add(light_var.add_effects(effects))

    for conf in config.get(CONF_ON_TURN_ON, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], light_var)
        await auto.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_TURN_OFF, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], light_var)
        await auto.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], light_var)
        await auto.build_automation(trigger, [], conf)

    if (color_correct := config.get(CONF_COLOR_CORRECT)) is not None:
        cg.add(output_var.set_correction(*color_correct))

    if (power_supply_id := config.get(CONF_POWER_SUPPLY)) is not None:
        var_ = await cg.get_variable(power_supply_id)
        cg.add(output_var.set_power_supply(var_))

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, light_var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(light_var, web_server_config)


async def register_light(output_var, config):
    light_var = cg.new_Pvariable(config[CONF_ID], output_var)
    queue_entity_register("light", config)
    CORE.register_platform_component("light", light_var)
    await cg.register_component(light_var, config)
    await setup_light_core_(light_var, config, output_var)


async def new_light(config, *args):
    output_var = cg.new_Pvariable(config[CONF_OUTPUT_ID], *args)
    await register_light(output_var, config)
    return output_var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(light_ns.using)


# light_json_schema.cpp is only used by mqtt and web_server, which both
# auto load json; USE_JSON alone is too broad since other components load it.
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {"light_json_schema.cpp": ("USE_MQTT", "USE_WEBSERVER")}
)
