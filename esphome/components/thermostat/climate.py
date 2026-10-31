from typing import Any, NamedTuple

from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_MODE,
    CONF_AWAY_CONFIG,
    CONF_COOL_ACTION,
    CONF_COOL_DEADBAND,
    CONF_COOL_MODE,
    CONF_COOL_OVERRUN,
    CONF_DEFAULT_MODE,
    CONF_DEFAULT_TARGET_TEMPERATURE_HIGH,
    CONF_DEFAULT_TARGET_TEMPERATURE_LOW,
    CONF_DRY_ACTION,
    CONF_DRY_MODE,
    CONF_FAN_MODE,
    CONF_FAN_MODE_AUTO_ACTION,
    CONF_FAN_MODE_DIFFUSE_ACTION,
    CONF_FAN_MODE_FOCUS_ACTION,
    CONF_FAN_MODE_HIGH_ACTION,
    CONF_FAN_MODE_LOW_ACTION,
    CONF_FAN_MODE_MEDIUM_ACTION,
    CONF_FAN_MODE_MIDDLE_ACTION,
    CONF_FAN_MODE_OFF_ACTION,
    CONF_FAN_MODE_ON_ACTION,
    CONF_FAN_MODE_QUIET_ACTION,
    CONF_FAN_ONLY_ACTION,
    CONF_FAN_ONLY_ACTION_USES_FAN_MODE_TIMER,
    CONF_FAN_ONLY_COOLING,
    CONF_FAN_ONLY_MODE,
    CONF_FAN_WITH_COOLING,
    CONF_FAN_WITH_HEATING,
    CONF_HEAT_ACTION,
    CONF_HEAT_COOL_MODE,
    CONF_HEAT_DEADBAND,
    CONF_HEAT_MODE,
    CONF_HEAT_OVERRUN,
    CONF_HUMIDITY_SENSOR,
    CONF_IDLE_ACTION,
    CONF_MAX_COOLING_RUN_TIME,
    CONF_MAX_HEATING_RUN_TIME,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_COOLING_OFF_TIME,
    CONF_MIN_COOLING_RUN_TIME,
    CONF_MIN_FAN_MODE_SWITCHING_TIME,
    CONF_MIN_FANNING_OFF_TIME,
    CONF_MIN_FANNING_RUN_TIME,
    CONF_MIN_HEATING_OFF_TIME,
    CONF_MIN_HEATING_RUN_TIME,
    CONF_MIN_IDLE_TIME,
    CONF_MIN_TEMPERATURE,
    CONF_MODE,
    CONF_NAME,
    CONF_OFF_MODE,
    CONF_PRESET,
    CONF_SENSOR,
    CONF_SET_POINT_MINIMUM_DIFFERENTIAL,
    CONF_STARTUP_DELAY,
    CONF_SUPPLEMENTAL_COOLING_ACTION,
    CONF_SUPPLEMENTAL_COOLING_DELTA,
    CONF_SUPPLEMENTAL_HEATING_ACTION,
    CONF_SUPPLEMENTAL_HEATING_DELTA,
    CONF_SWING_BOTH_ACTION,
    CONF_SWING_HORIZONTAL_ACTION,
    CONF_SWING_MODE,
    CONF_SWING_OFF_ACTION,
    CONF_SWING_VERTICAL_ACTION,
    CONF_TARGET_TEMPERATURE_CHANGE_ACTION,
    CONF_VISUAL,
)
from esphome.helpers import cpp_string_escape
from esphome.types import ConfigType

CONF_DEFAULT_PRESET = "default_preset"
CONF_HUMIDITY_CONTROL_DEHUMIDIFY_ACTION = "humidity_control_dehumidify_action"
CONF_HUMIDITY_CONTROL_HUMIDIFY_ACTION = "humidity_control_humidify_action"
CONF_HUMIDITY_CONTROL_OFF_ACTION = "humidity_control_off_action"
CONF_HUMIDITY_HYSTERESIS = "humidity_hysteresis"
CONF_ON_BOOT_RESTORE_FROM = "on_boot_restore_from"
CONF_PRESET_CHANGE = "preset_change"
CONF_TARGET_HUMIDITY_CHANGE_ACTION = "target_humidity_change_action"

CODEOWNERS = ["@kbx81"]

climate_ns = cg.esphome_ns.namespace("climate")
thermostat_ns = cg.esphome_ns.namespace("thermostat")
ThermostatClimate = thermostat_ns.class_(
    "ThermostatClimate", climate.Climate, cg.Component
)
ThermostatClimateTargetTempConfig = thermostat_ns.struct(
    "ThermostatClimateTargetTempConfig"
)
ThermostatPresetEntry = thermostat_ns.struct("ThermostatPresetEntry")
ThermostatCustomPresetEntry = thermostat_ns.struct("ThermostatCustomPresetEntry")
OnBootRestoreFrom = thermostat_ns.enum("OnBootRestoreFrom")
ON_BOOT_RESTORE_FROM = OnBootRestoreFrom.options("MEMORY", "DEFAULT_PRESET")
validate_on_boot_restore_from = cv.enum(ON_BOOT_RESTORE_FROM, upper=True)

ClimateMode = climate_ns.enum("ClimateMode")
CLIMATE_MODES = ClimateMode.options(
    "OFF",
    "HEAT_COOL",
    "COOL",
    "HEAT",
    "DRY",
    "FAN_ONLY",
    "AUTO",
    prefix="CLIMATE_MODE_",
)
validate_climate_mode = cv.enum(CLIMATE_MODES, upper=True)

ClimatePreset = climate_ns.enum("ClimatePreset")

# Actions a preset's mode, fan_mode or swing_mode needs
_MODE_ACTIONS: dict[str, tuple[str, ...]] = {
    "HEAT_COOL": (CONF_COOL_ACTION, CONF_HEAT_ACTION),
    "COOL": (CONF_COOL_ACTION,),
    "HEAT": (CONF_HEAT_ACTION,),
    "DRY": (CONF_DRY_ACTION,),
    "FAN_ONLY": (CONF_FAN_ONLY_ACTION,),
    "AUTO": (CONF_COOL_ACTION, CONF_HEAT_ACTION),
    "OFF": (),
}
_FAN_MODE_ACTIONS: dict[str, str] = {
    "ON": CONF_FAN_MODE_ON_ACTION,
    "OFF": CONF_FAN_MODE_OFF_ACTION,
    "AUTO": CONF_FAN_MODE_AUTO_ACTION,
    "LOW": CONF_FAN_MODE_LOW_ACTION,
    "MEDIUM": CONF_FAN_MODE_MEDIUM_ACTION,
    "HIGH": CONF_FAN_MODE_HIGH_ACTION,
    "MIDDLE": CONF_FAN_MODE_MIDDLE_ACTION,
    "FOCUS": CONF_FAN_MODE_FOCUS_ACTION,
    "DIFFUSE": CONF_FAN_MODE_DIFFUSE_ACTION,
    "QUIET": CONF_FAN_MODE_QUIET_ACTION,
}
_SWING_MODE_ACTIONS: dict[str, str] = {
    "BOTH": CONF_SWING_BOTH_ACTION,
    "HORIZONTAL": CONF_SWING_HORIZONTAL_ACTION,
    "OFF": CONF_SWING_OFF_ACTION,
    "VERTICAL": CONF_SWING_VERTICAL_ACTION,
}
_PRESET_MODE_ACTIONS: dict[str, dict[str, tuple[str, ...]]] = {
    CONF_MODE: _MODE_ACTIONS,
    CONF_FAN_MODE: {m: (k,) for m, k in _FAN_MODE_ACTIONS.items()},
    CONF_SWING_MODE: {m: (k,) for m, k in _SWING_MODE_ACTIONS.items()},
}


class _Trigger(NamedTuple):
    key: str
    getter: str
    supports: str | None = None  # set to true when the key is configured


# Generated in this order
_TRIGGERS: tuple[_Trigger, ...] = (
    _Trigger(CONF_COOL_ACTION, "get_cool_action_trigger", "set_supports_cool"),
    _Trigger(CONF_SUPPLEMENTAL_COOLING_ACTION, "get_supplemental_cool_action_trigger"),
    _Trigger(CONF_DRY_ACTION, "get_dry_action_trigger", "set_supports_dry"),
    _Trigger(
        CONF_FAN_ONLY_ACTION, "get_fan_only_action_trigger", "set_supports_fan_only"
    ),
    _Trigger(CONF_HEAT_ACTION, "get_heat_action_trigger", "set_supports_heat"),
    _Trigger(CONF_SUPPLEMENTAL_HEATING_ACTION, "get_supplemental_heat_action_trigger"),
    _Trigger(CONF_AUTO_MODE, "get_auto_mode_trigger", "set_supports_auto"),
    _Trigger(CONF_COOL_MODE, "get_cool_mode_trigger", "set_supports_cool"),
    _Trigger(CONF_DRY_MODE, "get_dry_mode_trigger", "set_supports_dry"),
    _Trigger(CONF_FAN_ONLY_MODE, "get_fan_only_mode_trigger", "set_supports_fan_only"),
    _Trigger(CONF_HEAT_MODE, "get_heat_mode_trigger", "set_supports_heat"),
    _Trigger(
        CONF_HEAT_COOL_MODE, "get_heat_cool_mode_trigger", "set_supports_heat_cool"
    ),
    _Trigger(CONF_OFF_MODE, "get_off_mode_trigger"),
    *(
        _Trigger(
            key,
            f"get_fan_mode_{mode.lower()}_trigger",
            f"set_supports_fan_mode_{mode.lower()}",
        )
        for mode, key in _FAN_MODE_ACTIONS.items()
    ),
    *(
        _Trigger(
            key,
            f"get_swing_mode_{mode.lower()}_trigger",
            f"set_supports_swing_mode_{mode.lower()}",
        )
        for mode, key in _SWING_MODE_ACTIONS.items()
    ),
    _Trigger(CONF_TARGET_HUMIDITY_CHANGE_ACTION, "get_humidity_change_trigger"),
    _Trigger(CONF_TARGET_TEMPERATURE_CHANGE_ACTION, "get_temperature_change_trigger"),
)
# Mutually exclusive; supports is set before the automation is built
_HUMIDITY_CONTROL_TRIGGERS: tuple[_Trigger, ...] = (
    _Trigger(
        CONF_HUMIDITY_CONTROL_DEHUMIDIFY_ACTION,
        "get_humidity_control_dehumidify_action_trigger",
        "set_supports_dehumidification",
    ),
    _Trigger(
        CONF_HUMIDITY_CONTROL_HUMIDIFY_ACTION,
        "get_humidity_control_humidify_action_trigger",
        "set_supports_humidification",
    ),
)

# Settings passed to their setter as is, in this order
_DEADBAND_OVERRUNS: dict[str, str] = {
    CONF_COOL_DEADBAND: "set_cool_deadband",
    CONF_COOL_OVERRUN: "set_cool_overrun",
    CONF_HEAT_DEADBAND: "set_heat_deadband",
    CONF_HEAT_OVERRUN: "set_heat_overrun",
}
_TIMERS: dict[str, str] = {
    CONF_MAX_COOLING_RUN_TIME: "set_cooling_maximum_run_time_in_sec",
    CONF_MAX_HEATING_RUN_TIME: "set_heating_maximum_run_time_in_sec",
    CONF_MIN_COOLING_OFF_TIME: "set_cooling_minimum_off_time_in_sec",
    CONF_MIN_COOLING_RUN_TIME: "set_cooling_minimum_run_time_in_sec",
    CONF_MIN_FAN_MODE_SWITCHING_TIME: "set_fan_mode_minimum_switching_time_in_sec",
    CONF_MIN_FANNING_OFF_TIME: "set_fanning_minimum_off_time_in_sec",
    CONF_MIN_FANNING_RUN_TIME: "set_fanning_minimum_run_time_in_sec",
    CONF_MIN_HEATING_OFF_TIME: "set_heating_minimum_off_time_in_sec",
    CONF_MIN_HEATING_RUN_TIME: "set_heating_minimum_run_time_in_sec",
}
_SUPPLEMENTAL_DELTAS: dict[str, str] = {
    CONF_SUPPLEMENTAL_COOLING_DELTA: "set_supplemental_cool_delta",
    CONF_SUPPLEMENTAL_HEATING_DELTA: "set_supplemental_heat_delta",
}
_FLAGS: dict[str, str] = {
    CONF_FAN_ONLY_ACTION_USES_FAN_MODE_TIMER: "set_supports_fan_only_action_uses_fan_mode_timer",
    CONF_FAN_ONLY_COOLING: "set_supports_fan_only_cooling",
    CONF_FAN_WITH_COOLING: "set_supports_fan_with_cooling",
    CONF_FAN_WITH_HEATING: "set_supports_fan_with_heating",
    CONF_STARTUP_DELAY: "set_use_startup_delay",
}
_SETTINGS: tuple[tuple[str, str], ...] = (
    *_DEADBAND_OVERRUNS.items(),
    *_TIMERS.items(),
    *_SUPPLEMENTAL_DELTAS.items(),
    (CONF_MIN_IDLE_TIME, "set_idle_minimum_time_in_sec"),
    *_FLAGS.items(),
)

_TARGET_TEMPERATURES = (
    CONF_DEFAULT_TARGET_TEMPERATURE_LOW,
    CONF_DEFAULT_TARGET_TEMPERATURE_HIGH,
)

PRESET_CONFIG_SCHEMA = cv.Schema(
    {
        # Still accepted for existing configs; presets are flash table entries, not variables.
        cv.GenerateID(): cv.declare_id(ThermostatClimateTargetTempConfig),
        cv.Required(CONF_NAME): cv.string_strict,
        cv.Optional(CONF_MODE): validate_climate_mode,
        cv.Optional(CONF_DEFAULT_TARGET_TEMPERATURE_HIGH): cv.temperature,
        cv.Optional(CONF_DEFAULT_TARGET_TEMPERATURE_LOW): cv.temperature,
        cv.Optional(CONF_FAN_MODE): climate.validate_climate_fan_mode,
        cv.Optional(CONF_SWING_MODE): climate.validate_climate_swing_mode,
    }
)


def validate_temperature_preset(
    preset: ConfigType,
    root_config: ConfigType,
    name: str,
    requirements: dict[str, list[str]],
) -> None:
    # verify temperature settings for the provided preset / default / away configuration
    for config_temp, req_actions in requirements.items():
        for req_action in req_actions:
            # verify corresponding default target temperature exists when a given climate action exists
            if config_temp not in preset and req_action in root_config:
                raise cv.Invalid(
                    f"{config_temp} must be defined in {name} config when using {req_action}"
                )
            # if a given climate action is NOT defined, it should not have a default target temperature
            if config_temp in preset and req_action not in root_config:
                raise cv.Invalid(
                    f"{config_temp} is defined in {name} config with no {req_action}"
                )


def generate_comparable_preset(config: ConfigType, name: str) -> str:
    comparable_preset = f"{CONF_PRESET}:\n  -  {CONF_NAME}: {name}\n"
    for key in _TARGET_TEMPERATURES:
        if key in config:
            comparable_preset += f"     {key}: {config[key]}\n"
    return comparable_preset


def validate_heat_cool_mode(value: Any) -> list:
    """Validate heat_cool_mode - accepts either True or an automation."""
    if value is True:
        # Convert True to empty automation list
        return []
    if value is False:
        raise cv.Invalid(
            "heat_cool_mode cannot be 'false'. Specify 'true' to enable the mode or provide an automation"
        )
    # Otherwise validate as automation
    return automation.validate_automation(single=True)(value)


_KEY_REQUIREMENTS: dict[str, list[str]] = {
    CONF_HEAT_COOL_MODE: [
        CONF_COOL_ACTION,
        CONF_HEAT_ACTION,
        CONF_MIN_COOLING_OFF_TIME,
        CONF_MIN_COOLING_RUN_TIME,
        CONF_MIN_HEATING_OFF_TIME,
        CONF_MIN_HEATING_RUN_TIME,
    ],
    CONF_COOL_MODE: [
        CONF_COOL_ACTION,
        CONF_MIN_COOLING_OFF_TIME,
        CONF_MIN_COOLING_RUN_TIME,
    ],
    CONF_DRY_MODE: [
        CONF_DRY_ACTION,
        CONF_MIN_COOLING_OFF_TIME,
        CONF_MIN_COOLING_RUN_TIME,
    ],
    CONF_FAN_ONLY_MODE: [
        CONF_FAN_ONLY_ACTION,
    ],
    CONF_HEAT_MODE: [
        CONF_HEAT_ACTION,
        CONF_MIN_HEATING_OFF_TIME,
        CONF_MIN_HEATING_RUN_TIME,
    ],
    CONF_COOL_ACTION: [
        CONF_MIN_COOLING_OFF_TIME,
        CONF_MIN_COOLING_RUN_TIME,
    ],
    CONF_DRY_ACTION: [
        CONF_MIN_COOLING_OFF_TIME,
        CONF_MIN_COOLING_RUN_TIME,
    ],
    CONF_HEAT_ACTION: [
        CONF_MIN_HEATING_OFF_TIME,
        CONF_MIN_HEATING_RUN_TIME,
    ],
    CONF_SUPPLEMENTAL_COOLING_ACTION: [
        CONF_COOL_ACTION,
        CONF_MAX_COOLING_RUN_TIME,
        CONF_MIN_COOLING_OFF_TIME,
        CONF_MIN_COOLING_RUN_TIME,
        CONF_SUPPLEMENTAL_COOLING_DELTA,
    ],
    CONF_SUPPLEMENTAL_HEATING_ACTION: [
        CONF_HEAT_ACTION,
        CONF_MAX_HEATING_RUN_TIME,
        CONF_MIN_HEATING_OFF_TIME,
        CONF_MIN_HEATING_RUN_TIME,
        CONF_SUPPLEMENTAL_HEATING_DELTA,
    ],
    CONF_MAX_COOLING_RUN_TIME: [
        CONF_COOL_ACTION,
        CONF_SUPPLEMENTAL_COOLING_ACTION,
        CONF_SUPPLEMENTAL_COOLING_DELTA,
    ],
    CONF_MAX_HEATING_RUN_TIME: [
        CONF_HEAT_ACTION,
        CONF_SUPPLEMENTAL_HEATING_ACTION,
        CONF_SUPPLEMENTAL_HEATING_DELTA,
    ],
    CONF_MIN_FANNING_OFF_TIME: [
        CONF_FAN_ONLY_ACTION,
    ],
    CONF_MIN_FANNING_RUN_TIME: [
        CONF_FAN_ONLY_ACTION,
    ],
    CONF_MIN_HEATING_OFF_TIME: [
        CONF_HEAT_ACTION,
    ],
    CONF_MIN_HEATING_RUN_TIME: [
        CONF_HEAT_ACTION,
    ],
    CONF_SUPPLEMENTAL_COOLING_DELTA: [
        CONF_COOL_ACTION,
        CONF_MAX_COOLING_RUN_TIME,
        CONF_SUPPLEMENTAL_COOLING_ACTION,
    ],
    CONF_SUPPLEMENTAL_HEATING_DELTA: [
        CONF_HEAT_ACTION,
        CONF_MAX_HEATING_RUN_TIME,
        CONF_SUPPLEMENTAL_HEATING_ACTION,
    ],
    CONF_HUMIDITY_CONTROL_DEHUMIDIFY_ACTION: [
        CONF_HUMIDITY_CONTROL_OFF_ACTION,
        CONF_HUMIDITY_SENSOR,
    ],
    CONF_HUMIDITY_CONTROL_HUMIDIFY_ACTION: [
        CONF_HUMIDITY_CONTROL_OFF_ACTION,
        CONF_HUMIDITY_SENSOR,
    ],
}


def validate_thermostat(config: ConfigType) -> ConfigType:
    if CONF_FAN_ONLY_ACTION in config:
        # determine validation requirements based on fan_only_action_uses_fan_mode_timer setting
        requirements = (
            [CONF_MIN_FAN_MODE_SWITCHING_TIME]
            if config[CONF_FAN_ONLY_ACTION_USES_FAN_MODE_TIMER]
            else [CONF_MIN_FANNING_OFF_TIME, CONF_MIN_FANNING_RUN_TIME]
        )
        for config_req_action in requirements:
            if config_req_action not in config:
                raise cv.Invalid(
                    f"{config_req_action} must be defined to use {CONF_FAN_ONLY_ACTION}"
                )

    # determine validation requirements based on fan_only_cooling setting
    requirements = {
        CONF_DEFAULT_TARGET_TEMPERATURE_HIGH: (
            [CONF_COOL_ACTION, CONF_FAN_ONLY_ACTION]
            if config[CONF_FAN_ONLY_COOLING]
            else [CONF_COOL_ACTION]
        ),
        CONF_DEFAULT_TARGET_TEMPERATURE_LOW: [CONF_HEAT_ACTION],
    }

    # Legacy high/low configs
    for key in _TARGET_TEMPERATURES:
        if key in config:
            comparable_preset = generate_comparable_preset(config, "Your new preset")
            raise cv.Invalid(
                f"{key} is no longer valid. Please switch to using a preset for an equivalent experience.\nEquivalent configuration:\n\n"
                f"{comparable_preset}"
            )

    # Legacy away mode - raise an error instructing the user to switch to presets
    if CONF_AWAY_CONFIG in config:
        comparable_preset = generate_comparable_preset(config[CONF_AWAY_CONFIG], "Away")

        raise cv.Invalid(
            f"{CONF_AWAY_CONFIG} is no longer valid. Please switch to using a preset named "
            "Away"
            " for an equivalent experience.\nEquivalent configuration:\n\n"
            f"{comparable_preset}"
        )

    # Validate temperature requirements for presets
    if CONF_PRESET in config:
        for preset_config in config[CONF_PRESET]:
            validate_temperature_preset(
                preset_config, config, preset_config[CONF_NAME], requirements
            )

    # Warn about using the removed CONF_DEFAULT_MODE and advise users
    if config[CONF_DEFAULT_MODE] is not None:
        raise cv.Invalid(
            f"{CONF_DEFAULT_MODE} is no longer valid. Please switch to using presets and specify a {CONF_DEFAULT_PRESET}"
        )

    # Verify that the modes for presets are valid given the configuration
    if CONF_PRESET in config:
        # Preset temperature vs Visual temperature validation

        # Default visual configuration from climate_traits.h
        visual_config = config.get(CONF_VISUAL, {})
        visual_min_temperature = visual_config.get(CONF_MIN_TEMPERATURE, 10.0)
        visual_max_temperature = visual_config.get(CONF_MAX_TEMPERATURE, 30.0)

        for preset_config in config[CONF_PRESET]:
            if CONF_DEFAULT_TARGET_TEMPERATURE_LOW in preset_config:
                preset_min_temperature = preset_config[
                    CONF_DEFAULT_TARGET_TEMPERATURE_LOW
                ]
                if preset_min_temperature < visual_min_temperature:
                    raise cv.Invalid(
                        f"{CONF_DEFAULT_TARGET_TEMPERATURE_LOW} for {preset_config[CONF_NAME]} is set to {preset_min_temperature} which is less than the visual minimum temperature of {visual_min_temperature}"
                    )

            if CONF_DEFAULT_TARGET_TEMPERATURE_HIGH in preset_config:
                preset_max_temperature = preset_config[
                    CONF_DEFAULT_TARGET_TEMPERATURE_HIGH
                ]
                if preset_max_temperature > visual_max_temperature:
                    raise cv.Invalid(
                        f"{CONF_DEFAULT_TARGET_TEMPERATURE_HIGH} for {preset_config[CONF_NAME]} is set to {preset_max_temperature} which is more than the visual maximum temperature of {visual_max_temperature}"
                    )

        # Mode, fan mode and swing mode validation
        for mode_key, mode_actions in _PRESET_MODE_ACTIONS.items():
            for preset_config in config[CONF_PRESET]:
                if (mode := preset_config.get(mode_key)) is None:
                    continue
                for req in mode_actions[mode]:
                    if req not in config:
                        raise cv.Invalid(
                            f"{mode_key} is set to {mode} for {preset_config[CONF_NAME]} but {req} is not present in the configuration"
                        )

    # If a default preset is requested then ensure that preset is defined
    if CONF_DEFAULT_PRESET in config:
        default_preset = config[CONF_DEFAULT_PRESET]

        if CONF_PRESET not in config:
            raise cv.Invalid(
                f"{CONF_DEFAULT_PRESET} is specified but no presets are defined"
            )

        presets = config[CONF_PRESET]
        if not any(preset[CONF_NAME] == default_preset for preset in presets):
            raise cv.Invalid(
                f"{CONF_DEFAULT_PRESET} set to '{default_preset}' but no such preset has been defined. Available presets: {[preset[CONF_NAME] for preset in presets]}"
            )

    # If restoring default preset on boot is true then ensure we have a default preset
    if (
        config.get(CONF_ON_BOOT_RESTORE_FROM) == "DEFAULT_PRESET"
        and CONF_DEFAULT_PRESET not in config
    ):
        raise cv.Invalid(
            f"{CONF_DEFAULT_PRESET} must be defined to use {CONF_ON_BOOT_RESTORE_FROM} in DEFAULT_PRESET mode"
        )

    for key in (CONF_FAN_WITH_COOLING, CONF_FAN_WITH_HEATING):
        if config[key] is True and CONF_FAN_ONLY_ACTION not in config:
            raise cv.Invalid(f"{CONF_FAN_ONLY_ACTION} must be defined to use {key}")

    # if min_fan_mode_switching_time is defined, at least one fan_mode action should be defined
    if CONF_MIN_FAN_MODE_SWITCHING_TIME in config and not any(
        key in config for key in _FAN_MODE_ACTIONS.values()
    ):
        raise cv.Invalid(
            f"At least one of {', '.join(_FAN_MODE_ACTIONS.values())} must be defined to use {CONF_MIN_FAN_MODE_SWITCHING_TIME}"
        )
    return config


CONFIG_SCHEMA = cv.All(
    climate.climate_schema(ThermostatClimate)
    .extend(
        {
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HUMIDITY_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_IDLE_ACTION): automation.validate_automation(single=True),
            **{
                cv.Optional(key): (
                    validate_heat_cool_mode
                    if key == CONF_HEAT_COOL_MODE
                    else automation.validate_automation(single=True)
                )
                for key, *_ in _TRIGGERS
            },
            **{
                cv.Exclusive(
                    key, group_of_exclusion="humidity_control"
                ): automation.validate_automation(single=True)
                for key, *_ in _HUMIDITY_CONTROL_TRIGGERS
            },
            cv.Optional(
                CONF_HUMIDITY_CONTROL_OFF_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_HUMIDITY_HYSTERESIS, default=1.0): cv.percentage,
            cv.Optional(CONF_DEFAULT_MODE, default=None): cv.valid,
            cv.Optional(CONF_DEFAULT_PRESET): cv.string,
            **{cv.Optional(key): cv.temperature for key in _TARGET_TEMPERATURES},
            cv.Optional(
                CONF_SET_POINT_MINIMUM_DIFFERENTIAL, default=0.5
            ): cv.temperature_delta,
            **{
                cv.Optional(key, default=0.5): cv.temperature_delta
                for key in _DEADBAND_OVERRUNS
            },
            **{cv.Optional(key): cv.positive_time_period_seconds for key in _TIMERS},
            cv.Required(CONF_MIN_IDLE_TIME): cv.positive_time_period_seconds,
            **{cv.Optional(key): cv.temperature_delta for key in _SUPPLEMENTAL_DELTAS},
            **{cv.Optional(key, default=False): cv.boolean for key in _FLAGS},
            cv.Optional(CONF_AWAY_CONFIG): cv.Schema(
                {cv.Optional(key): cv.temperature for key in _TARGET_TEMPERATURES}
            ),
            cv.Optional(CONF_PRESET): cv.All(
                cv.ensure_list(PRESET_CONFIG_SCHEMA), cv.Length(max=255)
            ),
            cv.Optional(CONF_ON_BOOT_RESTORE_FROM): validate_on_boot_restore_from,
            cv.Optional(CONF_PRESET_CHANGE): automation.validate_automation(
                single=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(
        CONF_COOL_ACTION, CONF_DRY_ACTION, CONF_FAN_ONLY_ACTION, CONF_HEAT_ACTION
    ),
    *(cv.key_requires(key, *required) for key, required in _KEY_REQUIREMENTS.items()),
    # Dry mode runs on the cooling timers, so a dehumidifier needs no cool_action
    cv.key_requires(
        (CONF_MIN_COOLING_OFF_TIME, CONF_MIN_COOLING_RUN_TIME),
        CONF_COOL_ACTION,
        CONF_DRY_ACTION,
        any_of=True,
    ),
    cv.key_requires(
        tuple(_FAN_MODE_ACTIONS.values()), CONF_MIN_FAN_MODE_SWITCHING_TIME
    ),
    validate_thermostat,
)


async def _build_trigger(
    var: cg.MockObj, config: ConfigType, key: str, getter: str
) -> None:
    # heat_cool_mode: true is an empty list and has no automation
    if conf := config.get(key):
        await automation.build_automation(getattr(var, getter)(), [], conf)


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    two_points_available = CONF_HEAT_ACTION in config and (
        CONF_COOL_ACTION in config
        or (config[CONF_FAN_ONLY_COOLING] and CONF_FAN_ONLY_ACTION in config)
    )
    if two_points_available:
        cg.add(var.set_supports_two_points(True))

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(
        var.set_set_point_minimum_differential(
            config[CONF_SET_POINT_MINIMUM_DIFFERENTIAL]
        )
    )
    cg.add(var.set_sensor(sens))

    await cg.add_variable(var.set_humidity_sensor, config.get(CONF_HUMIDITY_SENSOR))

    for key, setter in _SETTINGS:
        cg.add_optional(getattr(var, setter), config.get(key))

    await _build_trigger(var, config, CONF_IDLE_ACTION, "get_idle_action_trigger")
    for key, getter, supports in _TRIGGERS:
        await _build_trigger(var, config, key, getter)
        if supports is not None and key in config:
            cg.add(getattr(var, supports)(True))
    for key, getter, supports in _HUMIDITY_CONTROL_TRIGGERS:
        if key in config:
            cg.add(getattr(var, supports)(True))
        await _build_trigger(var, config, key, getter)
    await _build_trigger(
        var,
        config,
        CONF_HUMIDITY_CONTROL_OFF_ACTION,
        "get_humidity_control_off_action_trigger",
    )
    cg.add(var.set_humidity_hysteresis(config[CONF_HUMIDITY_HYSTERESIS]))

    if CONF_PRESET in config:
        # Presets live in shared PROGMEM tables; the thermostat keeps a pointer and a count.
        standard_presets: list[cg.StructInitializer] = []
        custom_presets: list[cg.StructInitializer] = []
        nan = float("nan")
        unset = cg.RawExpression("std::nullopt")

        for preset_config in config[CONF_PRESET]:
            name = preset_config[CONF_NAME]
            low = preset_config.get(CONF_DEFAULT_TARGET_TEMPERATURE_LOW)
            high = preset_config.get(CONF_DEFAULT_TARGET_TEMPERATURE_HIGH)
            if two_points_available:
                temperatures = (nan, low, high)
            else:
                temperatures = (high if high is not None else low, nan, nan)
            target = ThermostatClimateTargetTempConfig(
                *(nan if t is None else t for t in temperatures),
                preset_config.get(CONF_MODE, unset),
                preset_config.get(CONF_FAN_MODE, unset),
                preset_config.get(CONF_SWING_MODE, unset),
            )
            if (standard := climate.CLIMATE_PRESETS.get(name.upper())) is not None:
                standard_presets.append(
                    cg.StructInitializer(
                        ThermostatPresetEntry, ("preset", standard), ("config", target)
                    )
                )
            else:
                custom_presets.append(
                    cg.StructInitializer(
                        ThermostatCustomPresetEntry,
                        ("name", cg.RawExpression(cpp_string_escape(name))),
                        ("config", target),
                    )
                )

        if standard_presets:
            table = cg.shared_progmem_array(
                "thermostat_presets", ThermostatPresetEntry, standard_presets
            )
            cg.add(var.set_preset_config(table, len(standard_presets)))
        if custom_presets:
            table = cg.shared_progmem_array(
                "thermostat_custom_presets", ThermostatCustomPresetEntry, custom_presets
            )
            cg.add(var.set_custom_preset_config(table, len(custom_presets)))

    if (default_preset := config.get(CONF_DEFAULT_PRESET)) is not None:
        # a built in preset is passed as its enum value
        cg.add(
            var.set_default_preset(
                climate.CLIMATE_PRESETS.get(default_preset.upper(), default_preset)
            )
        )

    cg.add_optional(var.set_on_boot_restore_from, config.get(CONF_ON_BOOT_RESTORE_FROM))

    await _build_trigger(var, config, CONF_PRESET_CHANGE, "get_preset_change_trigger")
