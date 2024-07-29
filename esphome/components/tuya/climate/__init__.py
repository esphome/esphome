from esphome import pins
from esphome.components import climate
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_SWITCH_DATAPOINT,
    CONF_SUPPORTS_COOL,
    CONF_SUPPORTS_HEAT,
    CONF_PRESET,
    CONF_SWING_MODE,
    CONF_FAN_MODE,
    CONF_TEMPERATURE,
)
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@jesserockz"]

CONF_ACTIVE_STATE = "active_state"
CONF_DATAPOINT = "datapoint"
CONF_HEATING_VALUE = "heating_value"
CONF_COOLING_VALUE = "cooling_value"
CONF_DRYING_VALUE = "drying_value"
CONF_FANONLY_VALUE = "fanonly_value"
CONF_HEATING_STATE_PIN = "heating_state_pin"
CONF_COOLING_STATE_PIN = "cooling_state_pin"
CONF_TARGET_TEMPERATURE_DATAPOINT = "target_temperature_datapoint"
CONF_CURRENT_TEMPERATURE_DATAPOINT = "current_temperature_datapoint"
CONF_TEMPERATURE_MULTIPLIER = "temperature_multiplier"
CONF_CURRENT_TEMPERATURE_MULTIPLIER = "current_temperature_multiplier"
CONF_TARGET_TEMPERATURE_MULTIPLIER = "target_temperature_multiplier"
CONF_ECO = "eco"
CONF_SLEEP = "sleep"
CONF_SLEEP_DATAPOINT = "sleep_datapoint"
CONF_REPORTS_FAHRENHEIT = "reports_fahrenheit"
CONF_VERTICAL_DATAPOINT = "vertical_datapoint"
CONF_HORIZONTAL_DATAPOINT = "horizontal_datapoint"
CONF_LOW_VALUE = "low_value"
CONF_MEDIUM_VALUE = "medium_value"
CONF_MIDDLE_VALUE = "middle_value"
CONF_HIGH_VALUE = "high_value"
CONF_AUTO_VALUE = "auto_value"

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
        if not cooling_supported and CONF_ACTIVE_STATE in value:
            active_state_config = value[CONF_ACTIVE_STATE]
            if (
                CONF_COOLING_VALUE in active_state_config
                or CONF_COOLING_STATE_PIN in value
            ):
                raise cv.Invalid(
                    f"Device does not support cooling, but {CONF_COOLING_VALUE} or {CONF_COOLING_STATE_PIN} specified."
                    f" Please add '{CONF_SUPPORTS_COOL}: true' to your configuration."
                )
        elif cooling_supported and CONF_ACTIVE_STATE in value:
            active_state_config = value[CONF_ACTIVE_STATE]
            if (
                CONF_COOLING_VALUE not in active_state_config
                and CONF_COOLING_STATE_PIN not in value
            ):
                raise cv.Invalid(
                    f"Either {CONF_ACTIVE_STATE} {CONF_COOLING_VALUE} or {CONF_COOLING_STATE_PIN} is required if"
                    f" {CONF_SUPPORTS_COOL}: true' is in your configuration."
                )
    return value


ACTIVE_STATES = cv.Schema(
    {
        cv.Required(CONF_DATAPOINT): cv.uint8_t,
        cv.Optional(CONF_HEATING_VALUE, default=1): cv.uint8_t,
        cv.Optional(CONF_COOLING_VALUE): cv.uint8_t,
        cv.Optional(CONF_DRYING_VALUE): cv.uint8_t,
        cv.Optional(CONF_FANONLY_VALUE): cv.uint8_t,
    },
)


PRESETS = cv.Schema(
    {
        cv.Optional(CONF_ECO): {
            cv.Required(CONF_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_TEMPERATURE): cv.temperature,
        },
        cv.Optional(CONF_SLEEP): {
            cv.Required(CONF_DATAPOINT): cv.uint8_t,
        },
    },
)

FAN_MODES = cv.Schema(
    {
        cv.Required(CONF_DATAPOINT): cv.uint8_t,
        cv.Optional(CONF_AUTO_VALUE): cv.uint8_t,
        cv.Optional(CONF_LOW_VALUE): cv.uint8_t,
        cv.Optional(CONF_MEDIUM_VALUE): cv.uint8_t,
        cv.Optional(CONF_MIDDLE_VALUE): cv.uint8_t,
        cv.Optional(CONF_HIGH_VALUE): cv.uint8_t,
    }
)

SWING_MODES = cv.Schema(
    {
        cv.Optional(CONF_VERTICAL_DATAPOINT): cv.uint8_t,
        cv.Optional(CONF_HORIZONTAL_DATAPOINT): cv.uint8_t,
    },
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
            cv.Optional(CONF_FAN_MODE): FAN_MODES,
            cv.Optional(CONF_SWING_MODE): SWING_MODES,
            cv.Optional("active_state_datapoint"): cv.invalid(
                "'active_state_datapoint' has been moved inside of the 'active_state' config block as 'datapoint'"
            ),
            cv.Optional("active_state_heating_value"): cv.invalid(
                "'active_state_heating_value' has been moved inside of the 'active_state' config block as 'heating_value'"
            ),
            cv.Optional("active_state_cooling_value"): cv.invalid(
                "'active_state_cooling_value' has been moved inside of the 'active_state' config block as 'cooling_value'"
            ),
            cv.Optional("eco_datapoint"): cv.invalid(
                "'eco_datapoint' has been moved inside of the 'eco' config block under 'preset' as 'datapoint'"
            ),
            cv.Optional("eco_temperature"): cv.invalid(
                "'eco_temperature' has been moved inside of the 'eco' config block under 'preset' as 'temperature'"
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_TARGET_TEMPERATURE_DATAPOINT, CONF_SWITCH_DATAPOINT),
    validate_temperature_multipliers,
    validate_cooling_values,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))

    cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))
    cg.add(var.set_supports_cool(config[CONF_SUPPORTS_COOL]))
    if switch_datapoint := config.get(CONF_SWITCH_DATAPOINT):
        cg.add(var.set_switch_id(switch_datapoint))

    if heating_state_pin_config := config.get(CONF_HEATING_STATE_PIN):
        heating_state_pin = await cg.gpio_pin_expression(heating_state_pin_config)
        cg.add(var.set_heating_state_pin(heating_state_pin))
    if cooling_state_pin_config := config.get(CONF_COOLING_STATE_PIN):
        cooling_state_pin = await cg.gpio_pin_expression(cooling_state_pin_config)
        cg.add(var.set_cooling_state_pin(cooling_state_pin))
    if active_state_config := config.get(CONF_ACTIVE_STATE):
        cg.add(var.set_active_state_id(active_state_config.get(CONF_DATAPOINT)))
        if (heating_value := active_state_config.get(CONF_HEATING_VALUE)) is not None:
            cg.add(var.set_active_state_heating_value(heating_value))
        if (cooling_value := active_state_config.get(CONF_COOLING_VALUE)) is not None:
            cg.add(var.set_active_state_cooling_value(cooling_value))
        if (drying_value := active_state_config.get(CONF_DRYING_VALUE)) is not None:
            cg.add(var.set_active_state_drying_value(drying_value))
        if (fanonly_value := active_state_config.get(CONF_FANONLY_VALUE)) is not None:
            cg.add(var.set_active_state_fanonly_value(fanonly_value))

    if target_temperature_datapoint := config.get(CONF_TARGET_TEMPERATURE_DATAPOINT):
        cg.add(var.set_target_temperature_id(target_temperature_datapoint))
    if current_temperature_datapoint := config.get(CONF_CURRENT_TEMPERATURE_DATAPOINT):
        cg.add(var.set_current_temperature_id(current_temperature_datapoint))

    if temperature_multiplier := config.get(CONF_TEMPERATURE_MULTIPLIER):
        cg.add(var.set_target_temperature_multiplier(temperature_multiplier))
        cg.add(var.set_current_temperature_multiplier(temperature_multiplier))
    else:
        if current_temperature_multiplier := config.get(
            CONF_CURRENT_TEMPERATURE_MULTIPLIER
        ):
            cg.add(
                var.set_current_temperature_multiplier(current_temperature_multiplier)
            )
        if target_temperature_multiplier := config.get(
            CONF_TARGET_TEMPERATURE_MULTIPLIER
        ):
            cg.add(var.set_target_temperature_multiplier(target_temperature_multiplier))

    if config[CONF_REPORTS_FAHRENHEIT]:
        cg.add(var.set_reports_fahrenheit())

    if preset_config := config.get(CONF_PRESET, {}):
        if eco_config := preset_config.get(CONF_ECO, {}):
            cg.add(var.set_eco_id(eco_config.get(CONF_DATAPOINT)))
            if eco_temperature := eco_config.get(CONF_TEMPERATURE):
                cg.add(var.set_eco_temperature(eco_temperature))
        if sleep_config := preset_config.get(CONF_SLEEP, {}):
            cg.add(var.set_sleep_id(sleep_config.get(CONF_DATAPOINT)))

    if swing_mode_config := config.get(CONF_SWING_MODE):
        if swing_vertical_datapoint := swing_mode_config.get(CONF_VERTICAL_DATAPOINT):
            cg.add(var.set_swing_vertical_id(swing_vertical_datapoint))
        if swing_horizontal_datapoint := swing_mode_config.get(
            CONF_HORIZONTAL_DATAPOINT
        ):
            cg.add(var.set_swing_horizontal_id(swing_horizontal_datapoint))
    if fan_mode_config := config.get(CONF_FAN_MODE):
        cg.add(var.set_fan_speed_id(fan_mode_config.get(CONF_DATAPOINT)))
        if (fan_auto_value := fan_mode_config.get(CONF_AUTO_VALUE)) is not None:
            cg.add(var.set_fan_speed_auto_value(fan_auto_value))
        if (fan_low_value := fan_mode_config.get(CONF_LOW_VALUE)) is not None:
            cg.add(var.set_fan_speed_low_value(fan_low_value))
        if (fan_medium_value := fan_mode_config.get(CONF_MEDIUM_VALUE)) is not None:
            cg.add(var.set_fan_speed_medium_value(fan_medium_value))
        if (fan_middle_value := fan_mode_config.get(CONF_MIDDLE_VALUE)) is not None:
            cg.add(var.set_fan_speed_middle_value(fan_middle_value))
        if (fan_high_value := fan_mode_config.get(CONF_HIGH_VALUE)) is not None:
            cg.add(var.set_fan_speed_high_value(fan_high_value))
