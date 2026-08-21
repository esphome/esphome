from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, sensor
from esphome.components.climate import climate_ns
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION,
    CONF_CURRENT_TEMPERATURE,
    CONF_CUSTOM_FAN_MODE,
    CONF_CUSTOM_FAN_MODES,
    CONF_CUSTOM_PRESET,
    CONF_CUSTOM_PRESETS,
    CONF_FAN_MODE,
    CONF_HUMIDITY_SENSOR,
    CONF_ID,
    CONF_INITIAL_STATE,
    CONF_MODE,
    CONF_OPTIMISTIC,
    CONF_PRESET,
    CONF_RESTORE_MODE,
    CONF_SENSOR,
    CONF_SUPPORTED_FAN_MODES,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_PRESETS,
    CONF_SUPPORTED_SWING_MODES,
    CONF_SWING_MODE,
    CONF_TARGET_TEMPERATURE,
    CONF_TARGET_TEMPERATURE_HIGH,
    CONF_TARGET_TEMPERATURE_LOW,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

from .. import template_ns

CONF_CURRENT_HUMIDITY = "current_humidity"
CONF_TARGET_HUMIDITY = "target_humidity"
CONF_SUPPORTS_ACTION = "supports_action"
CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE = "supports_two_point_target_temperature"
CONF_SUPPORTS_TARGET_HUMIDITY = "supports_target_humidity"
CONF_SUPPORTS_CURRENT_TEMPERATURE = "supports_current_temperature"
CONF_SUPPORTS_CURRENT_HUMIDITY = "supports_current_humidity"
CONF_SET_MODE_ACTION = "set_mode_action"
CONF_SET_TARGET_TEMPERATURE_ACTION = "set_target_temperature_action"
CONF_SET_TARGET_TEMPERATURE_LOW_ACTION = "set_target_temperature_low_action"
CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION = "set_target_temperature_high_action"
CONF_SET_TARGET_HUMIDITY_ACTION = "set_target_humidity_action"
CONF_SET_FAN_MODE_ACTION = "set_fan_mode_action"
CONF_SET_CUSTOM_FAN_MODE_ACTION = "set_custom_fan_mode_action"
CONF_SET_SWING_MODE_ACTION = "set_swing_mode_action"
CONF_SET_PRESET_ACTION = "set_preset_action"
CONF_SET_CUSTOM_PRESET_ACTION = "set_custom_preset_action"

TemplateClimate = template_ns.class_("TemplateClimate", climate.Climate, cg.Component)
TemplateClimatePublishAction = template_ns.class_(
    "TemplateClimatePublishAction",
    automation.Action,
    cg.Parented.template(TemplateClimate),
)

TemplateClimateRestoreMode = template_ns.enum(
    "TemplateClimateRestoreMode", is_class=True
)
CLIMATE_RESTORE_MODES = {
    "NO_RESTORE": TemplateClimateRestoreMode.TEMPLATE_CLIMATE_RESTORE_MODE_NO_RESTORE,
    "RESTORE": TemplateClimateRestoreMode.TEMPLATE_CLIMATE_RESTORE_MODE_RESTORE,
}

# Per-field actions that forward a requested value on. The third item is the type of `x`.
SET_ACTIONS = (
    (CONF_SET_MODE_ACTION, "get_set_mode_trigger", climate.ClimateMode),
    (
        CONF_SET_TARGET_TEMPERATURE_ACTION,
        "get_set_target_temperature_trigger",
        cg.float_,
    ),
    (
        CONF_SET_TARGET_TEMPERATURE_LOW_ACTION,
        "get_set_target_temperature_low_trigger",
        cg.float_,
    ),
    (
        CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION,
        "get_set_target_temperature_high_trigger",
        cg.float_,
    ),
    (CONF_SET_TARGET_HUMIDITY_ACTION, "get_set_target_humidity_trigger", cg.float_),
    (CONF_SET_FAN_MODE_ACTION, "get_set_fan_mode_trigger", climate.ClimateFanMode),
    (
        CONF_SET_CUSTOM_FAN_MODE_ACTION,
        "get_set_custom_fan_mode_trigger",
        cg.StringRef,
    ),
    (
        CONF_SET_SWING_MODE_ACTION,
        "get_set_swing_mode_trigger",
        climate.ClimateSwingMode,
    ),
    (CONF_SET_PRESET_ACTION, "get_set_preset_trigger", climate.ClimatePreset),
    (CONF_SET_CUSTOM_PRESET_ACTION, "get_set_custom_preset_trigger", cg.StringRef),
)

# supports_* keys have no default so that an omitted key can mean "derive it from the sensor or
# set action that makes the trait useful", which is not expressible once a default fills it in.
DERIVED_SUPPORTS = (
    (CONF_SUPPORTS_CURRENT_TEMPERATURE, (CONF_SENSOR,)),
    (CONF_SUPPORTS_CURRENT_HUMIDITY, (CONF_HUMIDITY_SENSOR,)),
    (
        CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE,
        (
            CONF_SET_TARGET_TEMPERATURE_LOW_ACTION,
            CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION,
        ),
    ),
    (CONF_SUPPORTS_TARGET_HUMIDITY, (CONF_SET_TARGET_HUMIDITY_ACTION,)),
)


# Custom fan modes/presets are opaque user-defined strings with no build-time correctness check
# elsewhere (Climate::set_supported_custom_fan_modes()/set_supported_custom_presets() don't block
# empty entries), so reject empty ones here -- they could never be selected at runtime anyway.
validate_custom_climate_string = cv.All(cv.string_strict, cv.Length(min=1))


def _validate_two_point(config: ConfigType) -> ConfigType:
    has_low = CONF_TARGET_TEMPERATURE_LOW in config
    has_high = CONF_TARGET_TEMPERATURE_HIGH in config
    if has_low != has_high:
        raise cv.Invalid(
            f"'{CONF_TARGET_TEMPERATURE_LOW}' and '{CONF_TARGET_TEMPERATURE_HIGH}' must be used together"
        )
    if (has_low or has_high) and CONF_TARGET_TEMPERATURE in config:
        raise cv.Invalid(
            f"'{CONF_TARGET_TEMPERATURE}' cannot be used together with "
            f"'{CONF_TARGET_TEMPERATURE_LOW}'/'{CONF_TARGET_TEMPERATURE_HIGH}'"
        )
    return config


def _validate_set_actions(config: ConfigType) -> ConfigType:
    has_low = CONF_SET_TARGET_TEMPERATURE_LOW_ACTION in config
    has_high = CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION in config
    if has_low != has_high:
        raise cv.Invalid(
            f"'{CONF_SET_TARGET_TEMPERATURE_LOW_ACTION}' and "
            f"'{CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION}' must be used together"
        )
    if (has_low or has_high) and CONF_SET_TARGET_TEMPERATURE_ACTION in config:
        raise cv.Invalid(
            f"'{CONF_SET_TARGET_TEMPERATURE_ACTION}' cannot be used together with "
            f"'{CONF_SET_TARGET_TEMPERATURE_LOW_ACTION}'/'{CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION}'"
        )
    return config


def _resolve_supports(config: ConfigType) -> ConfigType:
    # An explicit true stays valid without either, since climate.template.publish can report the
    # value; an explicit false that contradicts the configuration is an error, not a silent override.
    for key, sources in DERIVED_SUPPORTS:
        configured = [source for source in sources if source in config]
        if key not in config:
            config[key] = bool(configured)
        elif not config[key] and configured:
            raise cv.Invalid(
                f"'{key}' cannot be false while '{configured[0]}' is configured",
                path=[key],
            )
    return config


def _validate_initial_state(config: ConfigType) -> ConfigType:
    # Climate keeps target_temperature and target_temperature_low in a union, so writing the wrong
    # one of the pair corrupts the setpoint with no runtime complaint.
    if (initial_state := config.get(CONF_INITIAL_STATE)) is None:
        return config

    two_point = config[CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE]
    if two_point and CONF_TARGET_TEMPERATURE in initial_state:
        raise cv.Invalid(
            f"'{CONF_TARGET_TEMPERATURE}' is not available while "
            f"'{CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE}' is enabled; use "
            f"'{CONF_TARGET_TEMPERATURE_LOW}'/'{CONF_TARGET_TEMPERATURE_HIGH}' instead",
            path=[CONF_INITIAL_STATE, CONF_TARGET_TEMPERATURE],
        )
    if not two_point:
        for key in (CONF_TARGET_TEMPERATURE_LOW, CONF_TARGET_TEMPERATURE_HIGH):
            if key in initial_state:
                raise cv.Invalid(
                    f"'{key}' requires '{CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE}' to be enabled",
                    path=[CONF_INITIAL_STATE, key],
                )
    if (
        CONF_TARGET_HUMIDITY in initial_state
        and not config[CONF_SUPPORTS_TARGET_HUMIDITY]
    ):
        raise cv.Invalid(
            f"'{CONF_TARGET_HUMIDITY}' requires '{CONF_SUPPORTS_TARGET_HUMIDITY}' to be enabled",
            path=[CONF_INITIAL_STATE, CONF_TARGET_HUMIDITY],
        )
    return config


# Same settable fields as climate.template.publish, minus current_temperature/current_humidity/
# action: those are reported values (from a sensor or the device), not meaningful static defaults.
INITIAL_STATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MODE): climate.validate_climate_mode,
            cv.Optional(CONF_TARGET_TEMPERATURE): cv.temperature,
            cv.Optional(CONF_TARGET_TEMPERATURE_LOW): cv.temperature,
            cv.Optional(CONF_TARGET_TEMPERATURE_HIGH): cv.temperature,
            cv.Optional(CONF_TARGET_HUMIDITY): cv.percentage_int,
            cv.Exclusive(CONF_FAN_MODE, "fan_mode"): climate.validate_climate_fan_mode,
            cv.Exclusive(
                CONF_CUSTOM_FAN_MODE, "fan_mode"
            ): validate_custom_climate_string,
            cv.Optional(CONF_SWING_MODE): climate.validate_climate_swing_mode,
            cv.Exclusive(CONF_PRESET, "preset"): climate.validate_climate_preset,
            cv.Exclusive(CONF_CUSTOM_PRESET, "preset"): validate_custom_climate_string,
        }
    ),
    _validate_two_point,
)

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(TemplateClimate)
    .extend(
        {
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HUMIDITY_SENSOR): cv.use_id(sensor.Sensor),
            # action only ever arrives through climate.template.publish, so unlike the other
            # supports_* keys there is no set action to derive it from.
            cv.Optional(CONF_SUPPORTS_ACTION, default=False): cv.boolean,
            cv.Optional(CONF_SUPPORTS_CURRENT_TEMPERATURE): cv.boolean,
            cv.Optional(CONF_SUPPORTS_CURRENT_HUMIDITY): cv.boolean,
            cv.Optional(CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE): cv.boolean,
            cv.Optional(CONF_SUPPORTS_TARGET_HUMIDITY): cv.boolean,
            cv.Required(CONF_SUPPORTED_MODES): cv.All(
                cv.ensure_list(climate.validate_climate_mode), cv.Unique()
            ),
            cv.Optional(CONF_SUPPORTED_FAN_MODES): cv.All(
                cv.ensure_list(climate.validate_climate_fan_mode), cv.Unique()
            ),
            cv.Optional(CONF_CUSTOM_FAN_MODES): cv.All(
                cv.ensure_list(validate_custom_climate_string), cv.Unique()
            ),
            cv.Optional(CONF_SUPPORTED_SWING_MODES): cv.All(
                cv.ensure_list(climate.validate_climate_swing_mode), cv.Unique()
            ),
            cv.Optional(CONF_SUPPORTED_PRESETS): cv.All(
                cv.ensure_list(climate.validate_climate_preset), cv.Unique()
            ),
            cv.Optional(CONF_CUSTOM_PRESETS): cv.All(
                cv.ensure_list(validate_custom_climate_string), cv.Unique()
            ),
            cv.Optional(CONF_OPTIMISTIC, default=True): cv.boolean,
            cv.Optional(CONF_RESTORE_MODE, default="RESTORE"): cv.enum(
                CLIMATE_RESTORE_MODES, upper=True
            ),
            cv.Optional(CONF_INITIAL_STATE): INITIAL_STATE_SCHEMA,
            cv.Optional(CONF_SET_MODE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(
                CONF_SET_TARGET_TEMPERATURE_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(
                CONF_SET_TARGET_TEMPERATURE_LOW_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(
                CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(
                CONF_SET_TARGET_HUMIDITY_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_SET_FAN_MODE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(
                CONF_SET_CUSTOM_FAN_MODE_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_SET_SWING_MODE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SET_PRESET_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SET_CUSTOM_PRESET_ACTION): automation.validate_automation(
                single=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _validate_set_actions,
    _resolve_supports,
    _validate_initial_state,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    if (sens := config.get(CONF_SENSOR)) is not None:
        cg.add(var.set_sensor(await cg.get_variable(sens)))

    if (sens := config.get(CONF_HUMIDITY_SENSOR)) is not None:
        cg.add(var.set_humidity_sensor(await cg.get_variable(sens)))

    for key, flag in (
        (CONF_SUPPORTS_ACTION, climate_ns.CLIMATE_SUPPORTS_ACTION),
        (
            CONF_SUPPORTS_CURRENT_TEMPERATURE,
            climate_ns.CLIMATE_SUPPORTS_CURRENT_TEMPERATURE,
        ),
        (CONF_SUPPORTS_CURRENT_HUMIDITY, climate_ns.CLIMATE_SUPPORTS_CURRENT_HUMIDITY),
        (
            CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE,
            climate_ns.CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE,
        ),
        (CONF_SUPPORTS_TARGET_HUMIDITY, climate_ns.CLIMATE_SUPPORTS_TARGET_HUMIDITY),
    ):
        if config[key]:
            cg.add(var.add_feature_flags(flag))

    for mode in config[CONF_SUPPORTED_MODES]:
        cg.add(var.add_supported_mode(mode))

    for mode in config.get(CONF_SUPPORTED_FAN_MODES, []):
        cg.add(var.add_supported_fan_mode(mode))

    if CONF_CUSTOM_FAN_MODES in config:
        cg.add(
            var.set_supported_custom_fan_modes(
                cg.ArrayInitializer(*config[CONF_CUSTOM_FAN_MODES])
            )
        )

    for mode in config.get(CONF_SUPPORTED_SWING_MODES, []):
        cg.add(var.add_supported_swing_mode(mode))

    for preset in config.get(CONF_SUPPORTED_PRESETS, []):
        cg.add(var.add_supported_preset(preset))

    if CONF_CUSTOM_PRESETS in config:
        cg.add(
            var.set_supported_custom_presets(
                cg.ArrayInitializer(*config[CONF_CUSTOM_PRESETS])
            )
        )

    for key, trigger_getter, arg_type in SET_ACTIONS:
        if (conf := config.get(key)) is not None:
            await automation.build_automation(
                getattr(var, trigger_getter)(), [(arg_type, "x")], conf
            )

    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if (initial_state := config.get(CONF_INITIAL_STATE)) is not None:
        if (v := initial_state.get(CONF_MODE)) is not None:
            cg.add(var.set_mode(v))
        if (v := initial_state.get(CONF_TARGET_TEMPERATURE)) is not None:
            cg.add(var.set_target_temperature(v))
        if (v := initial_state.get(CONF_TARGET_TEMPERATURE_LOW)) is not None:
            cg.add(var.set_target_temperature_low(v))
        if (v := initial_state.get(CONF_TARGET_TEMPERATURE_HIGH)) is not None:
            cg.add(var.set_target_temperature_high(v))
        if (v := initial_state.get(CONF_TARGET_HUMIDITY)) is not None:
            cg.add(var.set_target_humidity(v))
        if (v := initial_state.get(CONF_FAN_MODE)) is not None:
            cg.add(var.set_fan_mode(v))
        if (v := initial_state.get(CONF_CUSTOM_FAN_MODE)) is not None:
            cg.add(var.set_custom_fan_mode(v))
        if (v := initial_state.get(CONF_SWING_MODE)) is not None:
            cg.add(var.set_swing_mode(v))
        if (v := initial_state.get(CONF_PRESET)) is not None:
            cg.add(var.set_preset(v))
        if (v := initial_state.get(CONF_CUSTOM_PRESET)) is not None:
            cg.add(var.set_custom_preset(v))


CLIMATE_TEMPLATE_PUBLISH_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(TemplateClimate),
            cv.Optional(CONF_CURRENT_TEMPERATURE): cv.templatable(cv.temperature),
            cv.Optional(CONF_CURRENT_HUMIDITY): cv.templatable(cv.percentage_int),
            cv.Optional(CONF_TARGET_TEMPERATURE): cv.templatable(cv.temperature),
            cv.Optional(CONF_TARGET_TEMPERATURE_LOW): cv.templatable(cv.temperature),
            cv.Optional(CONF_TARGET_TEMPERATURE_HIGH): cv.templatable(cv.temperature),
            cv.Optional(CONF_TARGET_HUMIDITY): cv.templatable(cv.percentage_int),
            cv.Optional(CONF_MODE): cv.templatable(climate.validate_climate_mode),
            cv.Optional(CONF_ACTION): cv.templatable(climate.validate_climate_action),
            cv.Exclusive(CONF_FAN_MODE, "fan_mode"): cv.templatable(
                climate.validate_climate_fan_mode
            ),
            cv.Exclusive(CONF_CUSTOM_FAN_MODE, "fan_mode"): cv.templatable(
                validate_custom_climate_string
            ),
            cv.Optional(CONF_SWING_MODE): cv.templatable(
                climate.validate_climate_swing_mode
            ),
            cv.Exclusive(CONF_PRESET, "preset"): cv.templatable(
                climate.validate_climate_preset
            ),
            cv.Exclusive(CONF_CUSTOM_PRESET, "preset"): cv.templatable(
                validate_custom_climate_string
            ),
        }
    ),
    _validate_two_point,
)


@automation.register_action(
    "climate.template.publish",
    TemplateClimatePublishAction,
    CLIMATE_TEMPLATE_PUBLISH_ACTION_SCHEMA,
    synchronous=True,
)
async def climate_template_publish_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    if (v := config.get(CONF_CURRENT_TEMPERATURE)) is not None:
        cg.add(var.set_current_temperature(await cg.templatable(v, args, cg.float_)))
    if (v := config.get(CONF_CURRENT_HUMIDITY)) is not None:
        cg.add(var.set_current_humidity(await cg.templatable(v, args, cg.float_)))
    if (v := config.get(CONF_TARGET_TEMPERATURE)) is not None:
        cg.add(var.set_target_temperature(await cg.templatable(v, args, cg.float_)))
    if (v := config.get(CONF_TARGET_TEMPERATURE_LOW)) is not None:
        cg.add(var.set_target_temperature_low(await cg.templatable(v, args, cg.float_)))
    if (v := config.get(CONF_TARGET_TEMPERATURE_HIGH)) is not None:
        cg.add(
            var.set_target_temperature_high(await cg.templatable(v, args, cg.float_))
        )
    if (v := config.get(CONF_TARGET_HUMIDITY)) is not None:
        cg.add(var.set_target_humidity(await cg.templatable(v, args, cg.float_)))
    if (v := config.get(CONF_MODE)) is not None:
        cg.add(var.set_mode(await cg.templatable(v, args, climate.ClimateMode)))
    if (v := config.get(CONF_ACTION)) is not None:
        cg.add(var.set_action(await cg.templatable(v, args, climate.ClimateAction)))
    if (v := config.get(CONF_FAN_MODE)) is not None:
        cg.add(var.set_fan_mode(await cg.templatable(v, args, climate.ClimateFanMode)))
    if (v := config.get(CONF_CUSTOM_FAN_MODE)) is not None:
        cg.add(var.set_custom_fan_mode(await cg.templatable(v, args, cg.std_string)))
    if (v := config.get(CONF_SWING_MODE)) is not None:
        cg.add(
            var.set_swing_mode(await cg.templatable(v, args, climate.ClimateSwingMode))
        )
    if (v := config.get(CONF_PRESET)) is not None:
        cg.add(var.set_preset(await cg.templatable(v, args, climate.ClimatePreset)))
    if (v := config.get(CONF_CUSTOM_PRESET)) is not None:
        cg.add(var.set_custom_preset(await cg.templatable(v, args, cg.std_string)))

    return var
