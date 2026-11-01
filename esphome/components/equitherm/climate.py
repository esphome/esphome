from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, number, output, sensor
from esphome.components.pid.climate import (
    CONF_CONTROL_PARAMETERS,
    CONF_DEADBAND_PARAMETERS,
    CONF_DEFAULT_TARGET_TEMPERATURE,
    CONF_DERIVATIVE_AVERAGING_SAMPLES,
    CONF_HEAT_OUTPUT,
    CONF_KD,
    CONF_KD_MULTIPLIER,
    CONF_KI,
    CONF_KI_MULTIPLIER,
    CONF_KP,
    CONF_KP_MULTIPLIER,
    CONF_MAX_INTEGRAL,
    CONF_MIN_INTEGRAL,
    CONF_THRESHOLD_HIGH,
    CONF_THRESHOLD_LOW,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CONF_OUTDOOR_SENSOR = "outdoor_sensor"
CONF_INDOOR_SENSOR = "indoor_sensor"
CONF_FLOW_SETPOINT = "flow_setpoint"
CONF_MANUAL_FLOW_TEMP = "manual_flow_temp"
CONF_OUTPUT_PARAMETERS = "output_parameters"

# Heating curve parameters
CONF_HEAT_CURVE_COEFFICIENT = "heat_curve_coefficient"
CONF_HEAT_CURVE_EXPONENT = "heat_curve_exponent"
CONF_HEAT_CURVE_SHIFT = "heat_curve_shift"

CONF_MIN_FLOW_TEMP = "min_flow_temp"
CONF_MAX_FLOW_TEMP = "max_flow_temp"

# Output behavior parameters
CONF_WRITE_DEADBAND = "write_deadband"

# Action configuration
CONF_UPDATE_PID = "update_pid"

# Automation triggers
CONF_ON_HEATING_START = "on_heating_start"
CONF_ON_HEATING_STOP = "on_heating_stop"

equitherm_ns = cg.esphome_ns.namespace("equitherm")
EquithermClimate = equitherm_ns.class_(
    "EquithermClimate", climate.Climate, cg.Component
)

CONTROL_PARAMETERS_SCHEMA = cv.Schema(
    {
        # Heating curve parameters
        cv.Required(CONF_HEAT_CURVE_COEFFICIENT): cv.float_range(min=0.5, max=5.0),
        cv.Required(CONF_HEAT_CURVE_EXPONENT): cv.float_range(min=0.5, max=3.0),
        cv.Optional(CONF_HEAT_CURVE_SHIFT, default="0°C"): cv.temperature_delta,
        # PID parameters for room temperature correction
        cv.Optional(CONF_KP, default=0.0): cv.float_range(min=0.0),
        cv.Optional(CONF_KI, default=0.0): cv.float_range(min=0.0),
        cv.Optional(CONF_KD, default=0.0): cv.float_range(min=0.0),
        cv.Optional(CONF_MIN_INTEGRAL, default=-1): cv.float_,
        cv.Optional(CONF_MAX_INTEGRAL, default=1): cv.float_,
        cv.Optional(CONF_DERIVATIVE_AVERAGING_SAMPLES, default=1): cv.int_range(
            min=1, max=16
        ),
    }
)


def _validate_output_parameters(config: ConfigType) -> ConfigType:
    """Validate output_parameters."""
    # Validate min/max flow temp
    if config[CONF_MIN_FLOW_TEMP] >= config[CONF_MAX_FLOW_TEMP]:
        raise cv.Invalid(
            f"{CONF_MIN_FLOW_TEMP} ({config[CONF_MIN_FLOW_TEMP]}) must be less than "
            f"{CONF_MAX_FLOW_TEMP} ({config[CONF_MAX_FLOW_TEMP]})"
        )

    return config


OUTPUT_PARAMETERS_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_MIN_FLOW_TEMP): cv.temperature,
            cv.Required(CONF_MAX_FLOW_TEMP): cv.temperature,
            cv.Optional(CONF_WRITE_DEADBAND, default="0.05°C"): cv.temperature_delta,
        }
    ),
    _validate_output_parameters,
)


def _validate_deadband_thresholds(config: ConfigType) -> ConfigType:
    """Validate that threshold_high > threshold_low for a valid deadband range."""
    threshold_low = config[CONF_THRESHOLD_LOW]
    threshold_high = config[CONF_THRESHOLD_HIGH]
    if threshold_high <= threshold_low:
        raise cv.Invalid(
            f"threshold_high ({threshold_high}) must be greater than "
            f"threshold_low ({threshold_low})"
        )
    return config


DEADBAND_PARAMETERS_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_THRESHOLD_LOW): cv.temperature_delta,
            cv.Required(CONF_THRESHOLD_HIGH): cv.temperature_delta,
            cv.Optional(CONF_KP_MULTIPLIER, default=0.0): cv.float_range(min=0.0),
            cv.Optional(CONF_KI_MULTIPLIER, default=0.0): cv.float_range(min=0.0),
            cv.Optional(CONF_KD_MULTIPLIER, default=0.0): cv.float_range(min=0.0),
        }
    ),
    _validate_deadband_thresholds,
)

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(EquithermClimate)
    .extend(
        {
            cv.Required(CONF_OUTDOOR_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_INDOOR_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_DEFAULT_TARGET_TEMPERATURE): cv.temperature,
            cv.Optional(CONF_FLOW_SETPOINT): cv.use_id(number.Number),
            cv.Optional(CONF_MANUAL_FLOW_TEMP): cv.use_id(number.Number),
            cv.Optional(CONF_HEAT_OUTPUT): cv.use_id(output.FloatOutput),
            cv.Required(CONF_CONTROL_PARAMETERS): CONTROL_PARAMETERS_SCHEMA,
            cv.Required(CONF_OUTPUT_PARAMETERS): OUTPUT_PARAMETERS_SCHEMA,
            cv.Optional(CONF_DEADBAND_PARAMETERS): DEADBAND_PARAMETERS_SCHEMA,
            cv.Optional(CONF_ON_HEATING_START): automation.validate_automation({}),
            cv.Optional(CONF_ON_HEATING_STOP): automation.validate_automation({}),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_FLOW_SETPOINT, CONF_HEAT_OUTPUT),
)


async def to_code(config: ConfigType) -> None:
    outdoor = await cg.get_variable(config[CONF_OUTDOOR_SENSOR])
    indoor = await cg.get_variable(config[CONF_INDOOR_SENSOR])
    var = await climate.new_climate(config, outdoor, indoor)
    await cg.register_component(var, config)

    await cg.add_variable(var.set_flow_setpoint_output, config.get(CONF_FLOW_SETPOINT))
    if await cg.add_variable(var.set_heat_output, config.get(CONF_HEAT_OUTPUT)):
        cg.add_define("USE_EQUITHERM_HEAT_OUTPUT")
    await cg.add_variable(var.set_manual_flow_temp, config.get(CONF_MANUAL_FLOW_TEMP))
    cg.add(var.set_default_target_temperature(config[CONF_DEFAULT_TARGET_TEMPERATURE]))

    params = config[CONF_CONTROL_PARAMETERS]
    for key, setter in (
        (CONF_HEAT_CURVE_COEFFICIENT, var.set_heat_curve_coefficient),
        (CONF_HEAT_CURVE_EXPONENT, var.set_heat_curve_exponent),
        (CONF_HEAT_CURVE_SHIFT, var.set_heat_curve_shift),
        (CONF_KP, var.set_kp),
        (CONF_KI, var.set_ki),
        (CONF_KD, var.set_kd),
        (CONF_MIN_INTEGRAL, var.set_min_integral),
        (CONF_MAX_INTEGRAL, var.set_max_integral),
        (CONF_DERIVATIVE_AVERAGING_SAMPLES, var.set_derivative_samples),
    ):
        cg.add(setter(params[key]))

    params = config[CONF_OUTPUT_PARAMETERS]
    for key, setter in (
        (CONF_MIN_FLOW_TEMP, var.set_min_flow_temp),
        (CONF_MAX_FLOW_TEMP, var.set_max_flow_temp),
        (CONF_WRITE_DEADBAND, var.set_write_deadband),
    ):
        cg.add(setter(params[key]))

    if (params := config.get(CONF_DEADBAND_PARAMETERS)) is not None:
        for key, setter in (
            (CONF_THRESHOLD_HIGH, var.set_threshold_high),
            (CONF_THRESHOLD_LOW, var.set_threshold_low),
            (CONF_KP_MULTIPLIER, var.set_kp_multiplier),
            (CONF_KI_MULTIPLIER, var.set_ki_multiplier),
            (CONF_KD_MULTIPLIER, var.set_kd_multiplier),
        ):
            cg.add(setter(params[key]))

    for conf_key, method in (
        (CONF_ON_HEATING_START, "add_on_heating_start_callback"),
        (CONF_ON_HEATING_STOP, "add_on_heating_stop_callback"),
    ):
        for conf in config.get(conf_key, []):
            await automation.build_callback_automation(var, method, [], conf)


# Action: equitherm.force_recalculate
EQUITHERM_FORCE_RECALCULATE_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(EquithermClimate),
        cv.Optional(CONF_UPDATE_PID, default=True): cv.templatable(cv.boolean),
    }
)


automation.register_apply_action(
    "climate.equitherm.force_recalculate",
    EQUITHERM_FORCE_RECALCULATE_ACTION_SCHEMA,
    automation.ApplyField(CONF_UPDATE_PID, "force_recalculate", cg.bool_),
)
