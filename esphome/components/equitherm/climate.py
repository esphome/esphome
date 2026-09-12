from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, number, output, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@P4uLT"]

CONF_DEFAULT_TARGET_TEMPERATURE = "default_target_temperature"
CONF_OUTDOOR_SENSOR = "outdoor_sensor"
CONF_INDOOR_SENSOR = "indoor_sensor"
CONF_FLOW_SETPOINT = "flow_setpoint"
CONF_MANUAL_FLOW_TEMP = "manual_flow_temp"
CONF_HEAT_OUTPUT = "heat_output"
CONF_SENSOR_STALE_TIMEOUT = "sensor_stale_timeout"  # outdoor-only (back-compat)
CONF_INDOOR_FRESH_WINDOW = "indoor_fresh_window"
CONF_INDOOR_FAULT_HORIZON = "indoor_fault_horizon"
CONF_CONTROL_PARAMETERS = "control_parameters"
CONF_OUTPUT_PARAMETERS = "output_parameters"
CONF_DEADBAND_PARAMETERS = "deadband_parameters"

# Heating curve parameters
CONF_HEAT_CURVE_COEFFICIENT = "heat_curve_coefficient"
CONF_HEAT_CURVE_EXPONENT = "heat_curve_exponent"
CONF_HEAT_CURVE_SHIFT = "heat_curve_shift"

CONF_MIN_FLOW_TEMP = "min_flow_temp"
CONF_MAX_FLOW_TEMP = "max_flow_temp"

# Output behavior parameters
CONF_WRITE_DEADBAND = "write_deadband"

# PID parameters
CONF_KP = "kp"
CONF_KI = "ki"
CONF_KD = "kd"
CONF_MIN_INTEGRAL = "min_integral"
CONF_MAX_INTEGRAL = "max_integral"
CONF_DERIVATIVE_AVERAGING_SAMPLES = "derivative_averaging_samples"

# Deadband parameters
CONF_THRESHOLD_HIGH = "threshold_high"
CONF_THRESHOLD_LOW = "threshold_low"
CONF_KP_MULTIPLIER = "kp_multiplier"
CONF_KI_MULTIPLIER = "ki_multiplier"
CONF_KD_MULTIPLIER = "kd_multiplier"

# Action configuration
CONF_UPDATE_PID = "update_pid"

# Automation triggers
CONF_ON_HEATING_START = "on_heating_start"
CONF_ON_HEATING_STOP = "on_heating_stop"

equitherm_ns = cg.esphome_ns.namespace("equitherm")
EquithermClimate = equitherm_ns.class_(
    "EquithermClimate", climate.Climate, cg.Component
)
EquithermForceRecalculateAction = equitherm_ns.class_(
    "EquithermForceRecalculateAction", automation.Action
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


def _validate_output_parameters(config):
    """Validate output_parameters."""
    # Validate min/max flow temp
    if config[CONF_MIN_FLOW_TEMP] >= config[CONF_MAX_FLOW_TEMP]:
        raise cv.Invalid(
            f"{CONF_MIN_FLOW_TEMP} ({config[CONF_MIN_FLOW_TEMP]}) must be less than "
            f"{CONF_MAX_FLOW_TEMP} ({config[CONF_MAX_FLOW_TEMP]})"
        )

    return config


def _validate_indoor_timeouts(config):
    """fresh_window must be strictly less than fault_horizon."""
    fw = config[CONF_INDOOR_FRESH_WINDOW]
    fh = config[CONF_INDOOR_FAULT_HORIZON]
    if fw >= fh:
        raise cv.Invalid(
            f"{CONF_INDOOR_FRESH_WINDOW} ({fw}) must be less than "
            f"{CONF_INDOOR_FAULT_HORIZON} ({fh})"
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


def _validate_deadband_thresholds(config):
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
            cv.Optional(
                CONF_SENSOR_STALE_TIMEOUT, default="10min"
            ): cv.positive_time_period_milliseconds,  # outdoor-only now
            cv.Optional(
                CONF_INDOOR_FRESH_WINDOW, default="30min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_INDOOR_FAULT_HORIZON, default="4h"
            ): cv.positive_time_period_milliseconds,
            cv.Required(CONF_CONTROL_PARAMETERS): CONTROL_PARAMETERS_SCHEMA,
            cv.Required(CONF_OUTPUT_PARAMETERS): OUTPUT_PARAMETERS_SCHEMA,
            cv.Optional(CONF_DEADBAND_PARAMETERS): DEADBAND_PARAMETERS_SCHEMA,
            cv.Optional(CONF_ON_HEATING_START): automation.validate_automation({}),
            cv.Optional(CONF_ON_HEATING_STOP): automation.validate_automation({}),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_FLOW_SETPOINT, CONF_HEAT_OUTPUT),
    _validate_indoor_timeouts,
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    # Sensors
    outdoor = await cg.get_variable(config[CONF_OUTDOOR_SENSOR])
    cg.add(var.set_outdoor_sensor(outdoor))

    indoor = await cg.get_variable(config[CONF_INDOOR_SENSOR])
    cg.add(var.set_indoor_sensor(indoor))

    # Output (mutually exclusive)
    if CONF_FLOW_SETPOINT in config:
        flow_setpoint = await cg.get_variable(config[CONF_FLOW_SETPOINT])
        cg.add(var.set_flow_setpoint_output(flow_setpoint))
    if CONF_HEAT_OUTPUT in config:
        heat_output = await cg.get_variable(config[CONF_HEAT_OUTPUT])
        cg.add(var.set_heat_output(heat_output))
        cg.add_define("USE_EQUITHERM_HEAT_OUTPUT")

    # Manual flow temperature number entity (for CLIMATE_PRESET_MANUAL)
    if CONF_MANUAL_FLOW_TEMP in config:
        manual_flow_temp = await cg.get_variable(config[CONF_MANUAL_FLOW_TEMP])
        cg.add(var.set_manual_flow_temp(manual_flow_temp))

    # Climate defaults
    cg.add(var.set_default_target_temperature(config[CONF_DEFAULT_TARGET_TEMPERATURE]))

    # Sensor staleness / indoor health timeouts
    cg.add(var.set_outdoor_stale_timeout_ms(config[CONF_SENSOR_STALE_TIMEOUT]))
    cg.add(var.set_indoor_fresh_window_ms(config[CONF_INDOOR_FRESH_WINDOW]))
    cg.add(var.set_indoor_fault_horizon_ms(config[CONF_INDOOR_FAULT_HORIZON]))

    # Control parameters (heating curve + PID)
    params = config[CONF_CONTROL_PARAMETERS]
    cg.add(var.set_heat_curve_coefficient(params[CONF_HEAT_CURVE_COEFFICIENT]))
    cg.add(var.set_heat_curve_exponent(params[CONF_HEAT_CURVE_EXPONENT]))
    cg.add(var.set_heat_curve_shift(params[CONF_HEAT_CURVE_SHIFT]))
    # PID parameters
    cg.add(var.set_kp(params[CONF_KP]))
    cg.add(var.set_ki(params[CONF_KI]))
    cg.add(var.set_kd(params[CONF_KD]))
    cg.add(var.set_min_integral(params[CONF_MIN_INTEGRAL]))
    cg.add(var.set_max_integral(params[CONF_MAX_INTEGRAL]))
    cg.add(var.set_derivative_samples(params[CONF_DERIVATIVE_AVERAGING_SAMPLES]))

    # Output parameters
    params = config[CONF_OUTPUT_PARAMETERS]
    cg.add(var.set_min_flow_temp(params[CONF_MIN_FLOW_TEMP]))
    cg.add(var.set_max_flow_temp(params[CONF_MAX_FLOW_TEMP]))
    cg.add(var.set_write_deadband(params[CONF_WRITE_DEADBAND]))

    # Deadband parameters - optional
    if CONF_DEADBAND_PARAMETERS in config:
        params = config[CONF_DEADBAND_PARAMETERS]
        cg.add(var.set_threshold_high(params[CONF_THRESHOLD_HIGH]))
        cg.add(var.set_threshold_low(params[CONF_THRESHOLD_LOW]))
        cg.add(var.set_kp_multiplier(params[CONF_KP_MULTIPLIER]))
        cg.add(var.set_ki_multiplier(params[CONF_KI_MULTIPLIER]))
        cg.add(var.set_kd_multiplier(params[CONF_KD_MULTIPLIER]))

    # Automation triggers (callback-based, supports multiple entries)
    for conf in config.get(CONF_ON_HEATING_START, []):
        await automation.build_callback_automation(
            var, "add_on_heating_start_callback", [], conf
        )
    for conf in config.get(CONF_ON_HEATING_STOP, []):
        await automation.build_callback_automation(
            var, "add_on_heating_stop_callback", [], conf
        )


# Action: equitherm.force_recalculate
EQUITHERM_FORCE_RECALCULATE_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(EquithermClimate),
        cv.Optional(CONF_UPDATE_PID, default=True): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "climate.equitherm.force_recalculate",
    EquithermForceRecalculateAction,
    EQUITHERM_FORCE_RECALCULATE_ACTION_SCHEMA,
    synchronous=True,
)
async def equitherm_force_recalculate_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    update_pid_template_ = await cg.templatable(config[CONF_UPDATE_PID], args, bool)
    cg.add(var.set_update_pid(update_pid_template_))
    return var
