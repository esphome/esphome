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

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@jesserockz"]

CONF_ACTIVE_STATE_DATAPOINT = "active_state_datapoint"
CONF_ACTIVE_STATE_HEATING_VALUE = "active_state_heating_value"
CONF_ACTIVE_STATE_COOLING_VALUE = "active_state_cooling_value"
CONF_HEATING_STATE_PIN = "heating_state_pin"
CONF_COOLING_STATE_PIN = "cooling_state_pin"
CONF_TARGET_TEMPERATURE_DATAPOINT = "target_temperature_datapoint"
CONF_CURRENT_TEMPERATURE_DATAPOINT = "current_temperature_datapoint"
CONF_TEMPERATURE_MULTIPLIER = "temperature_multiplier"
CONF_CURRENT_TEMPERATURE_MULTIPLIER = "current_temperature_multiplier"
CONF_TARGET_TEMPERATURE_MULTIPLIER = "target_temperature_multiplier"
CONF_ECO_DATAPOINT = "eco_datapoint"
CONF_ECO_TEMPERATURE = "eco_temperature"

TuyaClimate = tuya_ns.class_("TuyaClimate", climate.Climate, cg.Component)


def validate_temperature_multipliers(value):
    if CONF_TEMPERATURE_MULTIPLIER in value:
        if (
            CONF_CURRENT_TEMPERATURE_MULTIPLIER in value
            or CONF_TARGET_TEMPERATURE_MULTIPLIER in value
        ):
            raise cv.Invalid(
                (
                    f"Cannot have {CONF_TEMPERATURE_MULTIPLIER} at the same time as "
                    f"{CONF_CURRENT_TEMPERATURE_MULTIPLIER} and "
                    f"{CONF_TARGET_TEMPERATURE_MULTIPLIER}"
                )
            )
    if (
        CONF_CURRENT_TEMPERATURE_MULTIPLIER in value
        and CONF_TARGET_TEMPERATURE_MULTIPLIER not in value
    ):
        raise cv.Invalid(
            (
                f"{CONF_TARGET_TEMPERATURE_MULTIPLIER} required if using "
                f"{CONF_CURRENT_TEMPERATURE_MULTIPLIER}"
            )
        )
    if (
        CONF_TARGET_TEMPERATURE_MULTIPLIER in value
        and CONF_CURRENT_TEMPERATURE_MULTIPLIER not in value
    ):
        raise cv.Invalid(
            (
                f"{CONF_CURRENT_TEMPERATURE_MULTIPLIER} required if using "
                f"{CONF_TARGET_TEMPERATURE_MULTIPLIER}"
            )
        )
    keys = (
        CONF_TEMPERATURE_MULTIPLIER,
        CONF_CURRENT_TEMPERATURE_MULTIPLIER,
        CONF_TARGET_TEMPERATURE_MULTIPLIER,
    )
    if all(multiplier not in value for multiplier in keys):
        value[CONF_TEMPERATURE_MULTIPLIER] = 1.0
    return value


def validate_active_state_values(value):
    if CONF_ACTIVE_STATE_DATAPOINT not in value:
        if CONF_ACTIVE_STATE_COOLING_VALUE in value:
            raise cv.Invalid(
                (
                    f"{CONF_ACTIVE_STATE_DATAPOINT} required if using "
                    f"{CONF_ACTIVE_STATE_COOLING_VALUE}"
                )
            )
    else:
        if value[CONF_SUPPORTS_COOL] and CONF_ACTIVE_STATE_COOLING_VALUE not in value:
            raise cv.Invalid(
                (
                    f"{CONF_ACTIVE_STATE_COOLING_VALUE} required if using "
                    f"{CONF_ACTIVE_STATE_DATAPOINT} and device supports cooling"
                )
            )
    return value


def validate_eco_values(value):
    if CONF_ECO_TEMPERATURE in value and CONF_ECO_DATAPOINT not in value:
        raise cv.Invalid(
            f"{CONF_ECO_DATAPOINT} required if using {CONF_ECO_TEMPERATURE}"
        )
    return value


CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TuyaClimate),
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Optional(CONF_SUPPORTS_HEAT, default=True): cv.boolean,
            cv.Optional(CONF_SUPPORTS_COOL, default=False): cv.boolean,
            cv.Optional(CONF_SWITCH_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_ACTIVE_STATE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_ACTIVE_STATE_HEATING_VALUE, default=1): cv.uint8_t,
            cv.Optional(CONF_ACTIVE_STATE_COOLING_VALUE): cv.uint8_t,
            cv.Optional(CONF_HEATING_STATE_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_COOLING_STATE_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_TARGET_TEMPERATURE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_CURRENT_TEMPERATURE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_TEMPERATURE_MULTIPLIER): cv.positive_float,
            cv.Optional(CONF_CURRENT_TEMPERATURE_MULTIPLIER): cv.positive_float,
            cv.Optional(CONF_TARGET_TEMPERATURE_MULTIPLIER): cv.positive_float,
            cv.Optional(CONF_ECO_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_ECO_TEMPERATURE): cv.temperature,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_TARGET_TEMPERATURE_DATAPOINT, CONF_SWITCH_DATAPOINT),
    validate_temperature_multipliers,
    validate_active_state_values,
    cv.has_at_most_one_key(CONF_ACTIVE_STATE_DATAPOINT, CONF_HEATING_STATE_PIN),
    cv.has_at_most_one_key(CONF_ACTIVE_STATE_DATAPOINT, CONF_COOLING_STATE_PIN),
    validate_eco_values,
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
    if CONF_ACTIVE_STATE_DATAPOINT in config:
        cg.add(var.set_active_state_id(config[CONF_ACTIVE_STATE_DATAPOINT]))
        if CONF_ACTIVE_STATE_HEATING_VALUE in config:
            cg.add(
                var.set_active_state_heating_value(
                    config[CONF_ACTIVE_STATE_HEATING_VALUE]
                )
            )
        if CONF_ACTIVE_STATE_COOLING_VALUE in config:
            cg.add(
                var.set_active_state_cooling_value(
                    config[CONF_ACTIVE_STATE_COOLING_VALUE]
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
    if CONF_ECO_DATAPOINT in config:
        cg.add(var.set_eco_id(config[CONF_ECO_DATAPOINT]))
        if CONF_ECO_TEMPERATURE in config:
            cg.add(var.set_eco_temperature(config[CONF_ECO_TEMPERATURE]))
