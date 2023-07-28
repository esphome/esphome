from esphome import pins
from esphome.components import climate
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_SWITCH_DATAPOINT,
    CONF_SUPPORTS_COOL,
    CONF_SUPPORTS_HEAT,
)
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya_new"]
CODEOWNERS = ["@jesserockz"]

CONF_ACTIVE_STATE = "active_state"
CONF_ACTIVE_STATE_DATAPOINT = "datapoint"
CONF_ACTIVE_STATE_HEATING_VALUE = "heating_value"
CONF_ACTIVE_STATE_COOLING_VALUE = "cooling_value"
CONF_ACTIVE_STATE_DRYING_VALUE = "drying_value"
CONF_ACTIVE_STATE_FANONLY_VALUE = "fanonly_value"
CONF_HEATING_STATE_PIN = "heating_state_pin"
CONF_COOLING_STATE_PIN = "cooling_state_pin"
CONF_TARGET_TEMPERATURE_DATAPOINT = "target_temperature_datapoint"
CONF_CURRENT_TEMPERATURE_DATAPOINT = "current_temperature_datapoint"
CONF_TEMPERATURE_MULTIPLIER = "temperature_multiplier"
CONF_CURRENT_TEMPERATURE_MULTIPLIER = "current_temperature_multiplier"
CONF_TARGET_TEMPERATURE_MULTIPLIER = "target_temperature_multiplier"
CONF_PRESET = "preset"
CONF_SLEEP_DATAPOINT = "sleep_datapoint"
CONF_ECO = "eco"
CONF_ECO_DATAPOINT = "datapoint"
CONF_ECO_TEMPERATURE = "temperature"
CONF_REPORTS_FAHRENHEIT = "reports_fahrenheit"
CONF_SWING_MODE = "swing_mode"
CONF_SWING_VERTICAL_DATAPOINT = "vertical_datapoint"
CONF_SWING_HORIZONTAL_DATAPOINT = "horizontal_datapoint"
CONF_FAN_SPEED = "fan_speed"
CONF_FAN_SPEED_DATAPOINT = "datapoint"
CONF_FAN_SPEED_LOW_VALUE = "low_value"
CONF_FAN_SPEED_MEDIUM_VALUE = "medium_value"
CONF_FAN_SPEED_MIDDLE_VALUE = "middle_value"
CONF_FAN_SPEED_HIGH_VALUE = "high_value"
CONF_FAN_SPEED_AUTO_VALUE = "auto_value"

TuyaClimate = tuya_ns.class_("TuyaClimate", climate.Climate, cg.Component)

def validate_temperature_multipliers(value):
    if CONF_TEMPERATURE_MULTIPLIER in value:
        if (
            CONF_CURRENT_TEMPERATURE_MULTIPLIER in value
            or CONF_TARGET_TEMPERATURE_MULTIPLIER in value
        ):
            raise cv.Invalid(
                f"Cannot have {CONF_TEMPERATURE_MULTIPLIER} at the same time as "
                f"{CONF_CURRENT_TEMPERATURE_MULTIPLIER} and "
                f"{CONF_TARGET_TEMPERATURE_MULTIPLIER}"
            )
    if (
        CONF_CURRENT_TEMPERATURE_MULTIPLIER in value
        and CONF_TARGET_TEMPERATURE_MULTIPLIER not in value
    ):
        raise cv.Invalid(
            f"{CONF_TARGET_TEMPERATURE_MULTIPLIER} required if using "
            f"{CONF_CURRENT_TEMPERATURE_MULTIPLIER}"
        )
    if (
        CONF_TARGET_TEMPERATURE_MULTIPLIER in value
        and CONF_CURRENT_TEMPERATURE_MULTIPLIER not in value
    ):
        raise cv.Invalid(
            f"{CONF_CURRENT_TEMPERATURE_MULTIPLIER} required if using "
            f"{CONF_TARGET_TEMPERATURE_MULTIPLIER}"
        )
    keys = (
        CONF_TEMPERATURE_MULTIPLIER,
        CONF_CURRENT_TEMPERATURE_MULTIPLIER,
        CONF_TARGET_TEMPERATURE_MULTIPLIER,
    )
    if all(multiplier not in value for multiplier in keys):
        value[CONF_TEMPERATURE_MULTIPLIER] = 1.0
    return value


def validate_cooling_values(value):
    if CONF_SUPPORTS_COOL in value:
        cooling_supported = value[CONF_SUPPORTS_COOL]
        if CONF_ACTIVE_STATE in value:
            active_state_config = value[CONF_ACTIVE_STATE]
            if CONF_ACTIVE_STATE_COOLING_VALUE in active_state_config and not cooling_supported:
                raise cv.Invalid(
                    f"device does not support cooling but "
                    f"{CONF_ACTIVE_STATE_COOLING_VALUE} specified"
                )
            if cooling_supported:
                if (
                    CONF_ACTIVE_STATE_COOLING_VALUE not in active_state_config
                    and CONF_COOLING_STATE_PIN not in value
                ):
                    raise cv.Invalid(
                        f"active_state {CONF_ACTIVE_STATE_COOLING_VALUE} or "
                        f"{CONF_COOLING_STATE_PIN} required if using "
                        f"{CONF_ACTIVE_STATE_DATAPOINT} and device supports cooling"
                    )
    return value

ACTIVE_STATES = cv.Schema(
    {
        cv.Required(CONF_ACTIVE_STATE_DATAPOINT): cv.uint8_t,
        cv.Optional(CONF_ACTIVE_STATE_HEATING_VALUE, default=1): cv.uint8_t,
        cv.Optional(CONF_ACTIVE_STATE_COOLING_VALUE): cv.uint8_t,
        cv.Optional(CONF_ACTIVE_STATE_DRYING_VALUE): cv.uint8_t,
        cv.Optional(CONF_ACTIVE_STATE_FANONLY_VALUE): cv.uint8_t,
    },
)


PRESETS = cv.Schema(
        {
            cv.Optional(CONF_ECO): {
                    cv.Required(CONF_ECO_DATAPOINT): cv.uint8_t,
                    cv.Optional(CONF_ECO_TEMPERATURE): cv.temperature,
                },
            cv.Optional(CONF_SLEEP_DATAPOINT): cv.uint8_t,
        },
)

FAN_SPEED_MODES = cv.Schema(
        {
            cv.Required(CONF_FAN_SPEED_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_FAN_SPEED_AUTO_VALUE): cv.uint8_t,
            cv.Optional(CONF_FAN_SPEED_LOW_VALUE): cv.uint8_t,
            cv.Optional(CONF_FAN_SPEED_MEDIUM_VALUE): cv.uint8_t,
            cv.Optional(CONF_FAN_SPEED_MIDDLE_VALUE): cv.uint8_t,
            cv.Optional(CONF_FAN_SPEED_HIGH_VALUE): cv.uint8_t,
        }
)

SWING_MODES = cv.Schema(
        {
            cv.Optional(CONF_SWING_VERTICAL_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_SWING_HORIZONTAL_DATAPOINT): cv.uint8_t,
        }
)

CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TuyaClimate),
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Optional(CONF_SUPPORTS_HEAT, default=True): cv.boolean,
            cv.Optional(CONF_SUPPORTS_COOL, default=False): cv.boolean,
            cv.Optional(CONF_SWITCH_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_ACTIVE_STATE): ACTIVE_STATES,
            cv.Optional(CONF_HEATING_STATE_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_COOLING_STATE_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_TARGET_TEMPERATURE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_CURRENT_TEMPERATURE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_TEMPERATURE_MULTIPLIER): cv.positive_float,
            cv.Optional(CONF_CURRENT_TEMPERATURE_MULTIPLIER): cv.positive_float,
            cv.Optional(CONF_TARGET_TEMPERATURE_MULTIPLIER): cv.positive_float,
            cv.Optional(CONF_REPORTS_FAHRENHEIT, default=False): cv.boolean,
            cv.Optional(CONF_PRESET): PRESETS,
            cv.Optional(CONF_FAN_SPEED): FAN_SPEED_MODES,
            cv.Optional(CONF_SWING_MODE): SWING_MODES,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_TARGET_TEMPERATURE_DATAPOINT, CONF_SWITCH_DATAPOINT),
    validate_temperature_multipliers,
    validate_cooling_values,
    cv.has_at_most_one_key(CONF_ACTIVE_STATE_DATAPOINT, CONF_HEATING_STATE_PIN),
    cv.has_at_most_one_key(CONF_ACTIVE_STATE_DATAPOINT, CONF_COOLING_STATE_PIN),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))

    cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))
    cg.add(var.set_supports_cool(config[CONF_SUPPORTS_COOL]))
    if CONF_SWITCH_DATAPOINT in config:
        cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
    if CONF_ACTIVE_STATE in config:
        active_state_config = config[CONF_ACTIVE_STATE]
        if CONF_ACTIVE_STATE_DATAPOINT in active_state_config:
            cg.add(
                var.set_active_state_id(active_state_config[CONF_ACTIVE_STATE_DATAPOINT])
            )
            if CONF_ACTIVE_STATE_HEATING_VALUE in active_state_config:
                cg.add(
                    var.set_active_state_heating_value(active_state_config[CONF_ACTIVE_STATE_HEATING_VALUE])
                )
            if CONF_ACTIVE_STATE_COOLING_VALUE in active_state_config:
                cg.add(
                    var.set_active_state_cooling_value(
                        active_state_config[CONF_ACTIVE_STATE_COOLING_VALUE]
                    )
                )
            if CONF_ACTIVE_STATE_DRYING_VALUE in active_state_config:
                cg.add(
                    var.set_active_state_drying_value(
                        active_state_config[CONF_ACTIVE_STATE_DRYING_VALUE]
                    )
                )
            if CONF_ACTIVE_STATE_FANONLY_VALUE in active_state_config:
                cg.add(
                    var.set_active_state_fanonly_value(
                        active_state_config[CONF_ACTIVE_STATE_FANONLY_VALUE]
                    )
                )
    else:
        if CONF_HEATING_STATE_PIN in config:
            heating_state_pin = await cg.gpio_pin_expression(
                config[CONF_HEATING_STATE_PIN]
            )
            cg.add(var.set_heating_state_pin(heating_state_pin))
        if CONF_COOLING_STATE_PIN in config:
            cooling_state_pin = await cg.gpio_pin_expression(
                config[CONF_COOLING_STATE_PIN]
            )
            cg.add(var.set_cooling_state_pin(cooling_state_pin))
    if CONF_TARGET_TEMPERATURE_DATAPOINT in config:
        cg.add(var.set_target_temperature_id(config[CONF_TARGET_TEMPERATURE_DATAPOINT]))
    if CONF_CURRENT_TEMPERATURE_DATAPOINT in config:
        cg.add(
            var.set_current_temperature_id(config[CONF_CURRENT_TEMPERATURE_DATAPOINT])
        )
    if CONF_TEMPERATURE_MULTIPLIER in config:
        cg.add(
            var.set_target_temperature_multiplier(config[CONF_TEMPERATURE_MULTIPLIER])
        )
        cg.add(
            var.set_current_temperature_multiplier(config[CONF_TEMPERATURE_MULTIPLIER])
        )
    else:
        cg.add(
            var.set_current_temperature_multiplier(
                config[CONF_CURRENT_TEMPERATURE_MULTIPLIER]
            )
        )
        cg.add(
            var.set_target_temperature_multiplier(
                config[CONF_TARGET_TEMPERATURE_MULTIPLIER]
            )
        )
    if config[CONF_REPORTS_FAHRENHEIT]:
        cg.add(var.set_reports_fahrenheit())
    
    preset_config = config.get(CONF_PRESET, {})
    if CONF_ECO in preset_config:
        eco_config = preset_config[CONF_ECO]
        eco_datapoint = eco_config.get(CONF_ECO_DATAPOINT)
        if eco_datapoint is not None:
            cg.add(var.set_eco_id(eco_datapoint))
        eco_temperature = eco_config.get(CONF_ECO_TEMPERATURE)
        if eco_temperature is not None:
            cg.add(var.set_eco_temperature(eco_temperature))

    if CONF_SLEEP_DATAPOINT in preset_config:
        cg.add(var.set_sleep_id(preset_config[CONF_SLEEP_DATAPOINT]))

    if CONF_SWING_MODE in config:
        swing_mode_config = config[CONF_SWING_MODE]
        if CONF_SWING_VERTICAL_DATAPOINT in swing_mode_config:
            cg.add(var.set_swing_vertical_id(swing_mode_config[CONF_SWING_VERTICAL_DATAPOINT]))
        if CONF_SWING_HORIZONTAL_DATAPOINT in swing_mode_config:
            cg.add(var.set_swing_horizontal_id(swing_mode_config[CONF_SWING_HORIZONTAL_DATAPOINT]))
    if CONF_FAN_SPEED in config:
        fan_speed_config = config[CONF_FAN_SPEED]
        if CONF_FAN_SPEED_DATAPOINT in fan_speed_config:
            cg.add(var.set_fan_speed_id(fan_speed_config[CONF_FAN_SPEED_DATAPOINT]))
            if CONF_FAN_SPEED_AUTO_VALUE in fan_speed_config:
                cg.add(var.set_fan_speed_auto_value(fan_speed_config[CONF_FAN_SPEED_AUTO_VALUE]))
            if CONF_FAN_SPEED_LOW_VALUE in fan_speed_config:
                cg.add(var.set_fan_speed_low_value(fan_speed_config[CONF_FAN_SPEED_LOW_VALUE]))
            if CONF_FAN_SPEED_MEDIUM_VALUE in fan_speed_config:
                cg.add(var.set_fan_speed_medium_value(fan_speed_config[CONF_FAN_SPEED_MEDIUM_VALUE]))
            if CONF_FAN_SPEED_MIDDLE_VALUE in fan_speed_config:
                cg.add(var.set_fan_speed_middle_value(fan_speed_config[CONF_FAN_SPEED_MIDDLE_VALUE]))
            if CONF_FAN_SPEED_HIGH_VALUE in fan_speed_config:
                cg.add(var.set_fan_speed_high_value(fan_speed_config[CONF_FAN_SPEED_HIGH_VALUE]))
