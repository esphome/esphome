import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import climate, sensor
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
    CONF_FAN_MODE_ON_ACTION,
    CONF_FAN_MODE_OFF_ACTION,
    CONF_FAN_MODE_AUTO_ACTION,
    CONF_FAN_MODE_LOW_ACTION,
    CONF_FAN_MODE_MEDIUM_ACTION,
    CONF_FAN_MODE_HIGH_ACTION,
    CONF_FAN_MODE_MIDDLE_ACTION,
    CONF_FAN_MODE_FOCUS_ACTION,
    CONF_FAN_MODE_DIFFUSE_ACTION,
    CONF_FAN_MODE_QUIET_ACTION,
    CONF_FAN_ONLY_ACTION,
    CONF_FAN_ONLY_ACTION_USES_FAN_MODE_TIMER,
    CONF_FAN_ONLY_COOLING,
    CONF_FAN_ONLY_MODE,
    CONF_FAN_WITH_COOLING,
    CONF_FAN_WITH_HEATING,
    CONF_HEAT_ACTION,
    CONF_HEAT_DEADBAND,
    CONF_HEAT_MODE,
    CONF_HEAT_OVERRUN,
    CONF_HUMIDITY_SENSOR,
    CONF_ID,
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
    CONF_NAME,
    CONF_MODE,
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

CONF_PRESET_CHANGE = "preset_change"
CONF_DEFAULT_PRESET = "default_preset"
CONF_ON_BOOT_RESTORE_FROM = "on_boot_restore_from"

CODEOWNERS = ["@kbx81"]

climate_ns = cg.esphome_ns.namespace("climate")
thermostat_ns = cg.esphome_ns.namespace("thermostat")
ThermostatClimate = thermostat_ns.class_(
    "ThermostatClimate", climate.Climate, cg.Component
)
ThermostatClimateTargetTempConfig = thermostat_ns.struct(
    "ThermostatClimateTargetTempConfig"
)
OnBootRestoreFrom = thermostat_ns.enum("OnBootRestoreFrom")
ON_BOOT_RESTORE_FROM = {
    "MEMORY": OnBootRestoreFrom.MEMORY,
    "DEFAULT_PRESET": OnBootRestoreFrom.DEFAULT_PRESET,
}
validate_on_boot_restore_from = cv.enum(ON_BOOT_RESTORE_FROM, upper=True)

ClimateMode = climate_ns.enum("ClimateMode")
CLIMATE_MODES = {
    "OFF": ClimateMode.CLIMATE_MODE_OFF,
    "HEAT_COOL": ClimateMode.CLIMATE_MODE_HEAT_COOL,
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
    "AUTO": ClimateMode.CLIMATE_MODE_AUTO,
}
validate_climate_mode = cv.enum(CLIMATE_MODES, upper=True)

ClimatePreset = climate_ns.enum("ClimatePreset")

PRESET_CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ThermostatClimateTargetTempConfig),
        cv.Required(CONF_NAME): cv.string_strict,
        cv.Optional(CONF_MODE): validate_climate_mode,
        cv.Optional(CONF_DEFAULT_TARGET_TEMPERATURE_HIGH): cv.temperature,
        cv.Optional(CONF_DEFAULT_TARGET_TEMPERATURE_LOW): cv.temperature,
        cv.Optional(CONF_FAN_MODE): cv.templatable(climate.validate_climate_fan_mode),
        cv.Optional(CONF_SWING_MODE): cv.templatable(
            climate.validate_climate_swing_mode
        ),
    }
)


def validate_temperature_preset(preset, root_config, name, requirements):
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


def generate_comparable_preset(config, name):
    comparable_preset = f"{CONF_PRESET}:\n" f"  -  {CONF_NAME}: {name}\n"

    if CONF_DEFAULT_TARGET_TEMPERATURE_LOW in config:
        comparable_preset += f"     {CONF_DEFAULT_TARGET_TEMPERATURE_LOW}: {config[CONF_DEFAULT_TARGET_TEMPERATURE_LOW]}\n"
    if CONF_DEFAULT_TARGET_TEMPERATURE_HIGH in config:
        comparable_preset += f"     {CONF_DEFAULT_TARGET_TEMPERATURE_HIGH}: {config[CONF_DEFAULT_TARGET_TEMPERATURE_HIGH]}\n"

    return comparable_preset


def validate_thermostat(config):
    # verify corresponding action(s) exist(s) for any defined climate mode or action
    requirements = {
        CONF_AUTO_MODE: [
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
        CONF_MIN_COOLING_OFF_TIME: [
            CONF_COOL_ACTION,
        ],
        CONF_MIN_COOLING_RUN_TIME: [
            CONF_COOL_ACTION,
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
    }
    for config_trigger, req_triggers in requirements.items():
        for req_trigger in req_triggers:
            if config_trigger in config and req_trigger not in config:
                raise cv.Invalid(
                    f"{req_trigger} must be defined to use {config_trigger}"
                )

    if CONF_FAN_ONLY_ACTION in config:
        # determine validation requirements based on fan_only_action_uses_fan_mode_timer setting
        if config[CONF_FAN_ONLY_ACTION_USES_FAN_MODE_TIMER] is True:
            requirements = [CONF_MIN_FAN_MODE_SWITCHING_TIME]
        else:
            requirements = [
                CONF_MIN_FANNING_OFF_TIME,
                CONF_MIN_FANNING_RUN_TIME,
            ]
        for config_req_action in requirements:
            if config_req_action not in config:
                raise cv.Invalid(
                    f"{config_req_action} must be defined to use {CONF_FAN_ONLY_ACTION}"
                )

    # for any fan_mode action, confirm min_fan_mode_switching_time is defined
    requirements = {
        CONF_MIN_FAN_MODE_SWITCHING_TIME: [
            CONF_FAN_MODE_ON_ACTION,
            CONF_FAN_MODE_OFF_ACTION,
            CONF_FAN_MODE_AUTO_ACTION,
            CONF_FAN_MODE_LOW_ACTION,
            CONF_FAN_MODE_MEDIUM_ACTION,
            CONF_FAN_MODE_HIGH_ACTION,
            CONF_FAN_MODE_MIDDLE_ACTION,
            CONF_FAN_MODE_FOCUS_ACTION,
            CONF_FAN_MODE_DIFFUSE_ACTION,
            CONF_FAN_MODE_QUIET_ACTION,
        ],
    }
    for req_config_item, config_triggers in requirements.items():
        for config_trigger in config_triggers:
            if config_trigger in config and req_config_item not in config:
                raise cv.Invalid(
                    f"{req_config_item} must be defined to use {config_trigger}"
                )

    # determine validation requirements based on fan_only_cooling setting
    if config[CONF_FAN_ONLY_COOLING] is True:
        requirements = {
            CONF_DEFAULT_TARGET_TEMPERATURE_HIGH: [
                CONF_COOL_ACTION,
                CONF_FAN_ONLY_ACTION,
            ],
            CONF_DEFAULT_TARGET_TEMPERATURE_LOW: [CONF_HEAT_ACTION],
        }
    else:
        requirements = {
            CONF_DEFAULT_TARGET_TEMPERATURE_HIGH: [CONF_COOL_ACTION],
            CONF_DEFAULT_TARGET_TEMPERATURE_LOW: [CONF_HEAT_ACTION],
        }

    # Legacy high/low configs
    if CONF_DEFAULT_TARGET_TEMPERATURE_LOW in config:
        comparable_preset = generate_comparable_preset(config, "Your new preset")

        raise cv.Invalid(
            f"{CONF_DEFAULT_TARGET_TEMPERATURE_LOW} is no longer valid. Please switch to using a preset for an equivalent experience.\nEquivalent configuration:\n\n"
            f"{comparable_preset}"
        )
    if CONF_DEFAULT_TARGET_TEMPERATURE_HIGH in config:
        comparable_preset = generate_comparable_preset(config, "Your new preset")

        raise cv.Invalid(
            f"{CONF_DEFAULT_TARGET_TEMPERATURE_HIGH} is no longer valid. Please switch to using a preset for an equivalent experience.\nEquivalent configuration:\n\n"
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
    if CONF_DEFAULT_MODE in config and config[CONF_DEFAULT_MODE] is not None:
        raise cv.Invalid(
            f"{CONF_DEFAULT_MODE} is no longer valid. Please switch to using presets and specify a {CONF_DEFAULT_PRESET}."
        )

    default_mode = config[CONF_DEFAULT_MODE]
    requirements = {
        "HEAT_COOL": [CONF_COOL_ACTION, CONF_HEAT_ACTION],
        "COOL": [CONF_COOL_ACTION],
        "HEAT": [CONF_HEAT_ACTION],
        "DRY": [CONF_DRY_ACTION],
        "FAN_ONLY": [CONF_FAN_ONLY_ACTION],
        "AUTO": [CONF_COOL_ACTION, CONF_HEAT_ACTION],
        "OFF": [],
    }
    actions_for_default_mode = requirements.get(default_mode, [])
    for req in actions_for_default_mode:
        if req not in config:
            raise cv.Invalid(
                f"{CONF_DEFAULT_MODE} is set to {default_mode} but {req} is not present in the configuration"
            )

    # Verify that the modes for presets are valid given the configuration
    if CONF_PRESET in config:
        # Preset temperature vs Visual temperature validation

        # Default visual configuration from climate_traits.h
        visual_min_temperature = 10.0
        visual_max_temperature = 30.0
        if CONF_VISUAL in config:
            visual_config = config[CONF_VISUAL]

            if CONF_MIN_TEMPERATURE in visual_config:
                visual_min_temperature = visual_config[CONF_MIN_TEMPERATURE]

            if CONF_MAX_TEMPERATURE in visual_config:
                visual_max_temperature = visual_config[CONF_MAX_TEMPERATURE]

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

        # Mode validation
        for preset_config in config[CONF_PRESET]:
            if CONF_MODE not in preset_config:
                continue

            mode = preset_config[CONF_MODE]

            for req in requirements[mode]:
                if req not in config:
                    raise cv.Invalid(
                        f"{CONF_MODE} is set to {mode} for {preset_config[CONF_NAME]} but {req} is not present in the configuration"
                    )

        # Fan mode requirements
        requirements = {
            "ON": [CONF_FAN_MODE_ON_ACTION],
            "OFF": [CONF_FAN_MODE_OFF_ACTION],
            "AUTO": [CONF_FAN_MODE_AUTO_ACTION],
            "LOW": [CONF_FAN_MODE_LOW_ACTION],
            "MEDIUM": [CONF_FAN_MODE_MEDIUM_ACTION],
            "HIGH": [CONF_FAN_MODE_HIGH_ACTION],
            "MIDDLE": [CONF_FAN_MODE_MIDDLE_ACTION],
            "FOCUS": [CONF_FAN_MODE_FOCUS_ACTION],
            "DIFFUSE": [CONF_FAN_MODE_DIFFUSE_ACTION],
            "QUIET": [CONF_FAN_MODE_QUIET_ACTION],
        }

        for preset_config in config[CONF_PRESET]:
            if CONF_FAN_MODE not in preset_config:
                continue

            fan_mode = preset_config[CONF_FAN_MODE]

            for req in requirements[fan_mode]:
                if req not in config:
                    raise cv.Invalid(
                        f"{CONF_FAN_MODE} is set to {fan_mode} for {preset_config[CONF_NAME]} but {req} is not present in the configuration"
                    )

        # Swing mode requirements
        requirements = {
            "OFF": [CONF_SWING_OFF_ACTION],
            "BOTH": [CONF_SWING_BOTH_ACTION],
            "VERTICAL": [CONF_SWING_VERTICAL_ACTION],
            "HORIZONTAL": [CONF_SWING_HORIZONTAL_ACTION],
        }

        for preset_config in config[CONF_PRESET]:
            if CONF_SWING_MODE not in preset_config:
                continue

            swing_mode = preset_config[CONF_SWING_MODE]

            for req in requirements[swing_mode]:
                if req not in config:
                    raise cv.Invalid(
                        f"{CONF_SWING_MODE} is set to {swing_mode} for {preset_config[CONF_NAME]} but {req} is not present in the configuration"
                    )

    # If a default preset is requested then ensure that preset is defined
    if CONF_DEFAULT_PRESET in config:
        default_preset = config[CONF_DEFAULT_PRESET]

        if CONF_PRESET not in config:
            raise cv.Invalid(
                f"{CONF_DEFAULT_PRESET} is specified but no presets are defined"
            )

        presets = config[CONF_PRESET]
        found_preset = False

        for preset in presets:
            if preset[CONF_NAME] == default_preset:
                found_preset = True
                break

        if found_preset is False:
            raise cv.Invalid(
                f"{CONF_DEFAULT_PRESET} set to '{default_preset}' but no such preset has been defined. Available presets: {[preset[CONF_NAME] for preset in presets]}"
            )

    # If restoring default preset on boot is true then ensure we have a default preset
    if (
        CONF_ON_BOOT_RESTORE_FROM in config
        and config[CONF_ON_BOOT_RESTORE_FROM] is OnBootRestoreFrom.DEFAULT_PRESET
    ):
        if CONF_DEFAULT_PRESET not in config:
            raise cv.Invalid(
                f"{CONF_DEFAULT_PRESET} must be defined to use {CONF_ON_BOOT_RESTORE_FROM} in DEFAULT_PRESET mode"
            )

    if config[CONF_FAN_WITH_COOLING] is True and CONF_FAN_ONLY_ACTION not in config:
        raise cv.Invalid(
            f"{CONF_FAN_ONLY_ACTION} must be defined to use {CONF_FAN_WITH_COOLING}"
        )
    if config[CONF_FAN_WITH_HEATING] is True and CONF_FAN_ONLY_ACTION not in config:
        raise cv.Invalid(
            f"{CONF_FAN_ONLY_ACTION} must be defined to use {CONF_FAN_WITH_HEATING}"
        )

    # if min_fan_mode_switching_time is defined, at least one fan_mode action should be defined
    if CONF_MIN_FAN_MODE_SWITCHING_TIME in config:
        requirements = [
            CONF_FAN_MODE_ON_ACTION,
            CONF_FAN_MODE_OFF_ACTION,
            CONF_FAN_MODE_AUTO_ACTION,
            CONF_FAN_MODE_LOW_ACTION,
            CONF_FAN_MODE_MEDIUM_ACTION,
            CONF_FAN_MODE_HIGH_ACTION,
            CONF_FAN_MODE_MIDDLE_ACTION,
            CONF_FAN_MODE_FOCUS_ACTION,
            CONF_FAN_MODE_DIFFUSE_ACTION,
            CONF_FAN_MODE_QUIET_ACTION,
        ]
        for config_req_action in requirements:
            if config_req_action in config:
                return config
        raise cv.Invalid(
            f"At least one of {CONF_FAN_MODE_ON_ACTION}, {CONF_FAN_MODE_OFF_ACTION}, {CONF_FAN_MODE_AUTO_ACTION}, {CONF_FAN_MODE_LOW_ACTION}, {CONF_FAN_MODE_MEDIUM_ACTION}, {CONF_FAN_MODE_HIGH_ACTION}, {CONF_FAN_MODE_MIDDLE_ACTION}, {CONF_FAN_MODE_FOCUS_ACTION}, {CONF_FAN_MODE_DIFFUSE_ACTION}, {CONF_FAN_MODE_QUIET_ACTION} must be defined to use {CONF_MIN_FAN_MODE_SWITCHING_TIME}"
        )
    return config


CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ThermostatClimate),
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HUMIDITY_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_IDLE_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_COOL_ACTION): automation.validate_automation(single=True),
            cv.Optional(
                CONF_SUPPLEMENTAL_COOLING_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_DRY_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_FAN_ONLY_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_HEAT_ACTION): automation.validate_automation(single=True),
            cv.Optional(
                CONF_SUPPLEMENTAL_HEATING_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_AUTO_MODE): automation.validate_automation(single=True),
            cv.Optional(CONF_COOL_MODE): automation.validate_automation(single=True),
            cv.Optional(CONF_DRY_MODE): automation.validate_automation(single=True),
            cv.Optional(CONF_FAN_ONLY_MODE): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_HEAT_MODE): automation.validate_automation(single=True),
            cv.Optional(CONF_OFF_MODE): automation.validate_automation(single=True),
            cv.Optional(CONF_FAN_MODE_ON_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_FAN_MODE_OFF_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_FAN_MODE_AUTO_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_FAN_MODE_LOW_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_FAN_MODE_MEDIUM_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_FAN_MODE_HIGH_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_FAN_MODE_MIDDLE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_FAN_MODE_FOCUS_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_FAN_MODE_DIFFUSE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_FAN_MODE_QUIET_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SWING_BOTH_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SWING_HORIZONTAL_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SWING_OFF_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SWING_VERTICAL_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(
                CONF_TARGET_TEMPERATURE_CHANGE_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_DEFAULT_MODE, default=None): cv.valid,
            cv.Optional(CONF_DEFAULT_PRESET): cv.templatable(cv.string),
            cv.Optional(CONF_DEFAULT_TARGET_TEMPERATURE_HIGH): cv.temperature,
            cv.Optional(CONF_DEFAULT_TARGET_TEMPERATURE_LOW): cv.temperature,
            cv.Optional(
                CONF_SET_POINT_MINIMUM_DIFFERENTIAL, default=0.5
            ): cv.temperature_delta,
            cv.Optional(CONF_COOL_DEADBAND, default=0.5): cv.temperature_delta,
            cv.Optional(CONF_COOL_OVERRUN, default=0.5): cv.temperature_delta,
            cv.Optional(CONF_HEAT_DEADBAND, default=0.5): cv.temperature_delta,
            cv.Optional(CONF_HEAT_OVERRUN, default=0.5): cv.temperature_delta,
            cv.Optional(CONF_MAX_COOLING_RUN_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_MAX_HEATING_RUN_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_MIN_COOLING_OFF_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_MIN_COOLING_RUN_TIME): cv.positive_time_period_seconds,
            cv.Optional(
                CONF_MIN_FAN_MODE_SWITCHING_TIME
            ): cv.positive_time_period_seconds,
            cv.Optional(CONF_MIN_FANNING_OFF_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_MIN_FANNING_RUN_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_MIN_HEATING_OFF_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_MIN_HEATING_RUN_TIME): cv.positive_time_period_seconds,
            cv.Required(CONF_MIN_IDLE_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_SUPPLEMENTAL_COOLING_DELTA): cv.temperature_delta,
            cv.Optional(CONF_SUPPLEMENTAL_HEATING_DELTA): cv.temperature_delta,
            cv.Optional(
                CONF_FAN_ONLY_ACTION_USES_FAN_MODE_TIMER, default=False
            ): cv.boolean,
            cv.Optional(CONF_FAN_ONLY_COOLING, default=False): cv.boolean,
            cv.Optional(CONF_FAN_WITH_COOLING, default=False): cv.boolean,
            cv.Optional(CONF_FAN_WITH_HEATING, default=False): cv.boolean,
            cv.Optional(CONF_STARTUP_DELAY, default=False): cv.boolean,
            cv.Optional(CONF_AWAY_CONFIG): cv.Schema(
                {
                    cv.Optional(CONF_DEFAULT_TARGET_TEMPERATURE_HIGH): cv.temperature,
                    cv.Optional(CONF_DEFAULT_TARGET_TEMPERATURE_LOW): cv.temperature,
                }
            ),
            cv.Optional(CONF_PRESET): cv.ensure_list(PRESET_CONFIG_SCHEMA),
            cv.Optional(CONF_ON_BOOT_RESTORE_FROM): validate_on_boot_restore_from,
            cv.Optional(CONF_PRESET_CHANGE): automation.validate_automation(
                single=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(
        CONF_COOL_ACTION, CONF_DRY_ACTION, CONF_FAN_ONLY_ACTION, CONF_HEAT_ACTION
    ),
    validate_thermostat,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    heat_cool_mode_available = CONF_HEAT_ACTION in config and CONF_COOL_ACTION in config
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

    if CONF_HUMIDITY_SENSOR in config:
        sens = await cg.get_variable(config[CONF_HUMIDITY_SENSOR])
        cg.add(var.set_humidity_sensor(sens))

    cg.add(var.set_cool_deadband(config[CONF_COOL_DEADBAND]))
    cg.add(var.set_cool_overrun(config[CONF_COOL_OVERRUN]))
    cg.add(var.set_heat_deadband(config[CONF_HEAT_DEADBAND]))
    cg.add(var.set_heat_overrun(config[CONF_HEAT_OVERRUN]))

    if CONF_MAX_COOLING_RUN_TIME in config:
        cg.add(
            var.set_cooling_maximum_run_time_in_sec(config[CONF_MAX_COOLING_RUN_TIME])
        )

    if CONF_MAX_HEATING_RUN_TIME in config:
        cg.add(
            var.set_heating_maximum_run_time_in_sec(config[CONF_MAX_HEATING_RUN_TIME])
        )

    if CONF_MIN_COOLING_OFF_TIME in config:
        cg.add(
            var.set_cooling_minimum_off_time_in_sec(config[CONF_MIN_COOLING_OFF_TIME])
        )

    if CONF_MIN_COOLING_RUN_TIME in config:
        cg.add(
            var.set_cooling_minimum_run_time_in_sec(config[CONF_MIN_COOLING_RUN_TIME])
        )

    if CONF_MIN_FAN_MODE_SWITCHING_TIME in config:
        cg.add(
            var.set_fan_mode_minimum_switching_time_in_sec(
                config[CONF_MIN_FAN_MODE_SWITCHING_TIME]
            )
        )

    if CONF_MIN_FANNING_OFF_TIME in config:
        cg.add(
            var.set_fanning_minimum_off_time_in_sec(config[CONF_MIN_FANNING_OFF_TIME])
        )

    if CONF_MIN_FANNING_RUN_TIME in config:
        cg.add(
            var.set_fanning_minimum_run_time_in_sec(config[CONF_MIN_FANNING_RUN_TIME])
        )

    if CONF_MIN_HEATING_OFF_TIME in config:
        cg.add(
            var.set_heating_minimum_off_time_in_sec(config[CONF_MIN_HEATING_OFF_TIME])
        )

    if CONF_MIN_HEATING_RUN_TIME in config:
        cg.add(
            var.set_heating_minimum_run_time_in_sec(config[CONF_MIN_HEATING_RUN_TIME])
        )

    if CONF_SUPPLEMENTAL_COOLING_DELTA in config:
        cg.add(var.set_supplemental_cool_delta(config[CONF_SUPPLEMENTAL_COOLING_DELTA]))

    if CONF_SUPPLEMENTAL_HEATING_DELTA in config:
        cg.add(var.set_supplemental_heat_delta(config[CONF_SUPPLEMENTAL_HEATING_DELTA]))

    cg.add(var.set_idle_minimum_time_in_sec(config[CONF_MIN_IDLE_TIME]))

    cg.add(
        var.set_supports_fan_only_action_uses_fan_mode_timer(
            config[CONF_FAN_ONLY_ACTION_USES_FAN_MODE_TIMER]
        )
    )
    cg.add(var.set_supports_fan_only_cooling(config[CONF_FAN_ONLY_COOLING]))
    cg.add(var.set_supports_fan_with_cooling(config[CONF_FAN_WITH_COOLING]))
    cg.add(var.set_supports_fan_with_heating(config[CONF_FAN_WITH_HEATING]))

    cg.add(var.set_use_startup_delay(config[CONF_STARTUP_DELAY]))

    await automation.build_automation(
        var.get_idle_action_trigger(), [], config[CONF_IDLE_ACTION]
    )

    if heat_cool_mode_available is True:
        cg.add(var.set_supports_heat_cool(True))
    else:
        cg.add(var.set_supports_heat_cool(False))

    if CONF_COOL_ACTION in config:
        await automation.build_automation(
            var.get_cool_action_trigger(), [], config[CONF_COOL_ACTION]
        )
        cg.add(var.set_supports_cool(True))
    if CONF_SUPPLEMENTAL_COOLING_ACTION in config:
        await automation.build_automation(
            var.get_supplemental_cool_action_trigger(),
            [],
            config[CONF_SUPPLEMENTAL_COOLING_ACTION],
        )
    if CONF_DRY_ACTION in config:
        await automation.build_automation(
            var.get_dry_action_trigger(), [], config[CONF_DRY_ACTION]
        )
        cg.add(var.set_supports_dry(True))
    if CONF_FAN_ONLY_ACTION in config:
        await automation.build_automation(
            var.get_fan_only_action_trigger(), [], config[CONF_FAN_ONLY_ACTION]
        )
        cg.add(var.set_supports_fan_only(True))
    if CONF_HEAT_ACTION in config:
        await automation.build_automation(
            var.get_heat_action_trigger(), [], config[CONF_HEAT_ACTION]
        )
        cg.add(var.set_supports_heat(True))
    if CONF_SUPPLEMENTAL_HEATING_ACTION in config:
        await automation.build_automation(
            var.get_supplemental_heat_action_trigger(),
            [],
            config[CONF_SUPPLEMENTAL_HEATING_ACTION],
        )
    if CONF_AUTO_MODE in config:
        await automation.build_automation(
            var.get_auto_mode_trigger(), [], config[CONF_AUTO_MODE]
        )
    if CONF_COOL_MODE in config:
        await automation.build_automation(
            var.get_cool_mode_trigger(), [], config[CONF_COOL_MODE]
        )
        cg.add(var.set_supports_cool(True))
    if CONF_DRY_MODE in config:
        await automation.build_automation(
            var.get_dry_mode_trigger(), [], config[CONF_DRY_MODE]
        )
        cg.add(var.set_supports_dry(True))
    if CONF_FAN_ONLY_MODE in config:
        await automation.build_automation(
            var.get_fan_only_mode_trigger(), [], config[CONF_FAN_ONLY_MODE]
        )
        cg.add(var.set_supports_fan_only(True))
    if CONF_HEAT_MODE in config:
        await automation.build_automation(
            var.get_heat_mode_trigger(), [], config[CONF_HEAT_MODE]
        )
        cg.add(var.set_supports_heat(True))
    if CONF_OFF_MODE in config:
        await automation.build_automation(
            var.get_off_mode_trigger(), [], config[CONF_OFF_MODE]
        )
    if CONF_FAN_MODE_ON_ACTION in config:
        await automation.build_automation(
            var.get_fan_mode_on_trigger(), [], config[CONF_FAN_MODE_ON_ACTION]
        )
        cg.add(var.set_supports_fan_mode_on(True))
    if CONF_FAN_MODE_OFF_ACTION in config:
        await automation.build_automation(
            var.get_fan_mode_off_trigger(), [], config[CONF_FAN_MODE_OFF_ACTION]
        )
        cg.add(var.set_supports_fan_mode_off(True))
    if CONF_FAN_MODE_AUTO_ACTION in config:
        await automation.build_automation(
            var.get_fan_mode_auto_trigger(), [], config[CONF_FAN_MODE_AUTO_ACTION]
        )
        cg.add(var.set_supports_fan_mode_auto(True))
    if CONF_FAN_MODE_LOW_ACTION in config:
        await automation.build_automation(
            var.get_fan_mode_low_trigger(), [], config[CONF_FAN_MODE_LOW_ACTION]
        )
        cg.add(var.set_supports_fan_mode_low(True))
    if CONF_FAN_MODE_MEDIUM_ACTION in config:
        await automation.build_automation(
            var.get_fan_mode_medium_trigger(), [], config[CONF_FAN_MODE_MEDIUM_ACTION]
        )
        cg.add(var.set_supports_fan_mode_medium(True))
    if CONF_FAN_MODE_HIGH_ACTION in config:
        await automation.build_automation(
            var.get_fan_mode_high_trigger(), [], config[CONF_FAN_MODE_HIGH_ACTION]
        )
        cg.add(var.set_supports_fan_mode_high(True))
    if CONF_FAN_MODE_MIDDLE_ACTION in config:
        await automation.build_automation(
            var.get_fan_mode_middle_trigger(), [], config[CONF_FAN_MODE_MIDDLE_ACTION]
        )
        cg.add(var.set_supports_fan_mode_middle(True))
    if CONF_FAN_MODE_FOCUS_ACTION in config:
        await automation.build_automation(
            var.get_fan_mode_focus_trigger(), [], config[CONF_FAN_MODE_FOCUS_ACTION]
        )
        cg.add(var.set_supports_fan_mode_focus(True))
    if CONF_FAN_MODE_DIFFUSE_ACTION in config:
        await automation.build_automation(
            var.get_fan_mode_diffuse_trigger(), [], config[CONF_FAN_MODE_DIFFUSE_ACTION]
        )
        cg.add(var.set_supports_fan_mode_diffuse(True))
    if CONF_FAN_MODE_QUIET_ACTION in config:
        await automation.build_automation(
            var.get_fan_mode_quiet_trigger(), [], config[CONF_FAN_MODE_QUIET_ACTION]
        )
        cg.add(var.set_supports_fan_mode_quiet(True))
    if CONF_SWING_BOTH_ACTION in config:
        await automation.build_automation(
            var.get_swing_mode_both_trigger(), [], config[CONF_SWING_BOTH_ACTION]
        )
        cg.add(var.set_supports_swing_mode_both(True))
    if CONF_SWING_HORIZONTAL_ACTION in config:
        await automation.build_automation(
            var.get_swing_mode_horizontal_trigger(),
            [],
            config[CONF_SWING_HORIZONTAL_ACTION],
        )
        cg.add(var.set_supports_swing_mode_horizontal(True))
    if CONF_SWING_OFF_ACTION in config:
        await automation.build_automation(
            var.get_swing_mode_off_trigger(), [], config[CONF_SWING_OFF_ACTION]
        )
        cg.add(var.set_supports_swing_mode_off(True))
    if CONF_SWING_VERTICAL_ACTION in config:
        await automation.build_automation(
            var.get_swing_mode_vertical_trigger(),
            [],
            config[CONF_SWING_VERTICAL_ACTION],
        )
        cg.add(var.set_supports_swing_mode_vertical(True))
    if CONF_TARGET_TEMPERATURE_CHANGE_ACTION in config:
        await automation.build_automation(
            var.get_temperature_change_trigger(),
            [],
            config[CONF_TARGET_TEMPERATURE_CHANGE_ACTION],
        )

    if CONF_PRESET in config:
        for preset_config in config[CONF_PRESET]:
            name = preset_config[CONF_NAME]
            standard_preset = None
            if name.upper() in climate.CLIMATE_PRESETS:
                standard_preset = climate.CLIMATE_PRESETS[name.upper()]

            if two_points_available is True:
                preset_target_config = ThermostatClimateTargetTempConfig(
                    preset_config[CONF_DEFAULT_TARGET_TEMPERATURE_LOW],
                    preset_config[CONF_DEFAULT_TARGET_TEMPERATURE_HIGH],
                )
            elif CONF_DEFAULT_TARGET_TEMPERATURE_HIGH in preset_config:
                preset_target_config = ThermostatClimateTargetTempConfig(
                    preset_config[CONF_DEFAULT_TARGET_TEMPERATURE_HIGH]
                )
            elif CONF_DEFAULT_TARGET_TEMPERATURE_LOW in preset_config:
                preset_target_config = ThermostatClimateTargetTempConfig(
                    preset_config[CONF_DEFAULT_TARGET_TEMPERATURE_LOW]
                )

            preset_target_variable = cg.new_variable(
                preset_config[CONF_ID], preset_target_config
            )

            if CONF_MODE in preset_config:
                cg.add(preset_target_variable.set_mode(preset_config[CONF_MODE]))

            if CONF_FAN_MODE in preset_config:
                cg.add(
                    preset_target_variable.set_fan_mode(preset_config[CONF_FAN_MODE])
                )

            if CONF_SWING_MODE in preset_config:
                cg.add(
                    preset_target_variable.set_swing_mode(
                        preset_config[CONF_SWING_MODE]
                    )
                )

            if standard_preset is not None:
                cg.add(var.set_preset_config(standard_preset, preset_target_variable))
            else:
                cg.add(var.set_custom_preset_config(name, preset_target_variable))

    if CONF_DEFAULT_PRESET in config:
        default_preset_name = config[CONF_DEFAULT_PRESET]

        # if the name is a built in preset use the appropriate naming format
        if default_preset_name.upper() in climate.CLIMATE_PRESETS:
            climate_preset = climate.CLIMATE_PRESETS[default_preset_name.upper()]
            cg.add(var.set_default_preset(climate_preset))
        else:
            cg.add(var.set_default_preset(default_preset_name))

    if CONF_ON_BOOT_RESTORE_FROM in config:
        cg.add(var.set_on_boot_restore_from(config[CONF_ON_BOOT_RESTORE_FROM]))

    if CONF_PRESET_CHANGE in config:
        await automation.build_automation(
            var.get_preset_change_trigger(), [], config[CONF_PRESET_CHANGE]
        )
