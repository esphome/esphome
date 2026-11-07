import esphome.codegen as cg
from esphome.components import water_heater
import esphome.config_validation as cv
from esphome.const import CONF_SUPPORTED_MODES, CONF_SWITCH_DATAPOINT
from esphome.types import ConfigType

from .. import CONF_TUYA_ID, Tuya, tuya_ns

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@iago-veiga"]

CONF_TARGET_TEMPERATURE_DATAPOINT = "target_temperature_datapoint"
CONF_CURRENT_TEMPERATURE_DATAPOINT = "current_temperature_datapoint"
CONF_TARGET_TEMPERATURE_MULTIPLIER = "target_temperature_multiplier"
CONF_CURRENT_TEMPERATURE_MULTIPLIER = "current_temperature_multiplier"
CONF_MODE_DATAPOINT = "mode_datapoint"

# Optional enum values that map a Tuya mode datapoint value to a WaterHeaterMode.
# Mirrors the "*_value" style used by the tuya climate fan modes.
CONF_ECO_VALUE = "eco_value"
CONF_ELECTRIC_VALUE = "electric_value"
CONF_PERFORMANCE_VALUE = "performance_value"
CONF_HIGH_DEMAND_VALUE = "high_demand_value"
CONF_HEAT_PUMP_VALUE = "heat_pump_value"
CONF_GAS_VALUE = "gas_value"

# Map of config key -> C++ setter name, one per non-OFF WaterHeaterMode. OFF is not an enum
# value: it is represented by the switch datapoint being off, just like the tuya climate.
MODE_VALUES = {
    CONF_ECO_VALUE: "set_eco_value",
    CONF_ELECTRIC_VALUE: "set_electric_value",
    CONF_PERFORMANCE_VALUE: "set_performance_value",
    CONF_HIGH_DEMAND_VALUE: "set_high_demand_value",
    CONF_HEAT_PUMP_VALUE: "set_heat_pump_value",
    CONF_GAS_VALUE: "set_gas_value",
}

TuyaWaterHeater = tuya_ns.class_(
    "TuyaWaterHeater", water_heater.WaterHeater, cg.Component
)


def _validate(config: ConfigType) -> ConfigType:
    # A mode datapoint is only useful if at least one mode value is mapped, and mode values
    # only make sense together with a mode datapoint.
    has_mode_values = any(key in config for key in MODE_VALUES)
    if CONF_MODE_DATAPOINT in config and not has_mode_values:
        raise cv.Invalid(
            f"'{CONF_MODE_DATAPOINT}' requires at least one mode value "
            f"(e.g. '{CONF_ECO_VALUE}' or '{CONF_ELECTRIC_VALUE}')"
        )
    if has_mode_values and CONF_MODE_DATAPOINT not in config:
        raise cv.Invalid(f"Mode values require '{CONF_MODE_DATAPOINT}' to be set")
    return config


CONFIG_SCHEMA = cv.All(
    water_heater.water_heater_schema(TuyaWaterHeater)
    .extend(
        {
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Required(CONF_SWITCH_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_TARGET_TEMPERATURE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_CURRENT_TEMPERATURE_DATAPOINT): cv.uint8_t,
            cv.Optional(
                CONF_TARGET_TEMPERATURE_MULTIPLIER, default=1.0
            ): cv.positive_float,
            cv.Optional(
                CONF_CURRENT_TEMPERATURE_MULTIPLIER, default=1.0
            ): cv.positive_float,
            cv.Optional(CONF_MODE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_ECO_VALUE): cv.uint8_t,
            cv.Optional(CONF_ELECTRIC_VALUE): cv.uint8_t,
            cv.Optional(CONF_PERFORMANCE_VALUE): cv.uint8_t,
            cv.Optional(CONF_HIGH_DEMAND_VALUE): cv.uint8_t,
            cv.Optional(CONF_HEAT_PUMP_VALUE): cv.uint8_t,
            cv.Optional(CONF_GAS_VALUE): cv.uint8_t,
            cv.Optional(CONF_SUPPORTED_MODES): cv.ensure_list(
                water_heater.validate_water_heater_mode
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = await water_heater.new_water_heater(config)
    await cg.register_component(var, config)

    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))

    cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))

    if (target_temp_dp := config.get(CONF_TARGET_TEMPERATURE_DATAPOINT)) is not None:
        cg.add(var.set_target_temperature_id(target_temp_dp))
    if (current_temp_dp := config.get(CONF_CURRENT_TEMPERATURE_DATAPOINT)) is not None:
        cg.add(var.set_current_temperature_id(current_temp_dp))

    cg.add(
        var.set_target_temperature_multiplier(
            config[CONF_TARGET_TEMPERATURE_MULTIPLIER]
        )
    )
    cg.add(
        var.set_current_temperature_multiplier(
            config[CONF_CURRENT_TEMPERATURE_MULTIPLIER]
        )
    )

    if (mode_dp := config.get(CONF_MODE_DATAPOINT)) is not None:
        cg.add(var.set_mode_id(mode_dp))
        for key, setter in MODE_VALUES.items():
            if (value := config.get(key)) is not None:
                cg.add(getattr(var, setter)(value))

    if (supported_modes := config.get(CONF_SUPPORTED_MODES)) is not None:
        cg.add(var.set_supported_modes(supported_modes))
