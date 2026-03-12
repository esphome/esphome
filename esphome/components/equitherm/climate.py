import esphome.codegen as cg
from esphome.components import climate, number, output, sensor
import esphome.config_validation as cv

CODEOWNERS = ["@P4uLT"]

CONF_DEFAULT_TARGET_TEMPERATURE = "default_target_temperature"
CONF_OUTDOOR_SENSOR = "outdoor_sensor"
CONF_INDOOR_SENSOR = "indoor_sensor"
CONF_CH_SETPOINT = "ch_setpoint"
CONF_HEAT_OUTPUT = "heat_output"
CONF_FALLBACK_OUTDOOR_TEMP = "fallback_outdoor_temp"
CONF_CONTROL_PARAMETERS = "control_parameters"
CONF_OUTPUT_PARAMETERS = "output_parameters"
CONF_DEADBAND_PARAMETERS = "deadband_parameters"

# Heating curve parameters (industry standard)
CONF_HC = "hc"  # Heat curve coefficient (0.5-1.5)
CONF_N = "n"  # Radiator exponent (1.2-1.33 for panels, 1.0 for underfloor)
CONF_SHIFT = "shift"

CONF_MIN_FLOW_TEMP = "min_flow_temp"
CONF_MAX_FLOW_TEMP = "max_flow_temp"
CONF_RATE_LIMIT_PER_MINUTE = "rate_limit_per_minute"

# PID parameters
CONF_KP = "kp"
CONF_KI = "ki"
CONF_KD = "kd"
CONF_MIN_INTEGRAL = "min_integral"
CONF_MAX_INTEGRAL = "max_integral"

# Deadband parameters
CONF_THRESHOLD_HIGH = "threshold_high"
CONF_THRESHOLD_LOW = "threshold_low"
CONF_KP_MULTIPLIER = "kp_multiplier"
CONF_KI_MULTIPLIER = "ki_multiplier"
CONF_KD_MULTIPLIER = "kd_multiplier"

equitherm_ns = cg.esphome_ns.namespace("equitherm")
EquithermClimate = equitherm_ns.class_(
    "EquithermClimate", climate.Climate, cg.Component
)

CONTROL_PARAMETERS_SCHEMA = cv.Schema(
    {
        # Industry-standard heating curve parameters
        cv.Required(CONF_HC): cv.float_range(min=0.1),
        cv.Required(CONF_N): cv.float_range(min=0.5, max=3.0),
        cv.Optional(CONF_SHIFT, default=0.0): cv.float_,
        # PID parameters for room temperature correction
        # Default kp=1.0 provides simple proportional correction (like old target_diff_factor)
        cv.Optional(CONF_KP, default=0.0): cv.float_range(min=0.0),
        cv.Optional(CONF_KI, default=0.0): cv.float_range(min=0.0),
        cv.Optional(CONF_KD, default=0.0): cv.float_range(min=0.0),
        cv.Optional(CONF_MIN_INTEGRAL, default=-10.0): cv.float_,
        cv.Optional(CONF_MAX_INTEGRAL, default=10.0): cv.float_,
    }
)

OUTPUT_PARAMETERS_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(
                CONF_MIN_FLOW_TEMP,
            ): cv.temperature,
            cv.Required(
                CONF_MAX_FLOW_TEMP,
            ): cv.temperature,
            cv.Optional(CONF_RATE_LIMIT_PER_MINUTE, default=0.2): cv.float_range(
                min=0.0, max=2.0
            ),
        }
    ),
    lambda config: (
        config
        if config[CONF_MIN_FLOW_TEMP] < config[CONF_MAX_FLOW_TEMP]
        else cv.Invalid(
            f"{CONF_MIN_FLOW_TEMP} ({config[CONF_MIN_FLOW_TEMP]}) must be less than {CONF_MAX_FLOW_TEMP} ({config[CONF_MAX_FLOW_TEMP]})"
        )
    ),
)

# =============================================================================
# Validation
# =============================================================================


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
            cv.Required(CONF_THRESHOLD_LOW): cv.float_,
            cv.Required(CONF_THRESHOLD_HIGH): cv.float_,
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
            cv.Optional(CONF_CH_SETPOINT): cv.use_id(number.Number),
            cv.Optional(CONF_HEAT_OUTPUT): cv.use_id(output.FloatOutput),
            cv.Optional(CONF_FALLBACK_OUTDOOR_TEMP, default=0.0): cv.temperature,
            cv.Required(CONF_CONTROL_PARAMETERS): CONTROL_PARAMETERS_SCHEMA,
            cv.Required(CONF_OUTPUT_PARAMETERS): OUTPUT_PARAMETERS_SCHEMA,
            cv.Optional(CONF_DEADBAND_PARAMETERS): DEADBAND_PARAMETERS_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_CH_SETPOINT, CONF_HEAT_OUTPUT),
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
    if CONF_CH_SETPOINT in config:
        ch_setpoint = await cg.get_variable(config[CONF_CH_SETPOINT])
        cg.add(var.set_ch_setpoint(ch_setpoint))
    if CONF_HEAT_OUTPUT in config:
        heat_output = await cg.get_variable(config[CONF_HEAT_OUTPUT])
        cg.add(var.set_heat_output(heat_output))

    # Climate defaults
    cg.add(var.set_default_target_temperature(config[CONF_DEFAULT_TARGET_TEMPERATURE]))

    # Fallback outdoor temperature (sensor failure handling)
    cg.add(var.set_fallback_outdoor_temp(config[CONF_FALLBACK_OUTDOOR_TEMP]))

    # Control parameters (heating curve + PID)
    params = config[CONF_CONTROL_PARAMETERS]
    cg.add(var.set_hc(params[CONF_HC]))
    cg.add(var.set_n(params[CONF_N]))
    cg.add(var.set_shift(params[CONF_SHIFT]))
    # PID parameters
    cg.add(var.set_kp(params[CONF_KP]))
    cg.add(var.set_ki(params[CONF_KI]))
    cg.add(var.set_kd(params[CONF_KD]))
    cg.add(var.set_min_integral(params[CONF_MIN_INTEGRAL]))
    cg.add(var.set_max_integral(params[CONF_MAX_INTEGRAL]))

    # Output parameters
    params = config[CONF_OUTPUT_PARAMETERS]
    cg.add(var.set_min_flow_temp(params[CONF_MIN_FLOW_TEMP]))
    cg.add(var.set_max_flow_temp(params[CONF_MAX_FLOW_TEMP]))
    cg.add(var.set_rate_limit_per_minute(params[CONF_RATE_LIMIT_PER_MINUTE]))

    # Deadband parameters - optional
    if CONF_DEADBAND_PARAMETERS in config:
        params = config[CONF_DEADBAND_PARAMETERS]
        cg.add(var.set_threshold_high(params[CONF_THRESHOLD_HIGH]))
        cg.add(var.set_threshold_low(params[CONF_THRESHOLD_LOW]))
        cg.add(var.set_kp_multiplier(params[CONF_KP_MULTIPLIER]))
        cg.add(var.set_ki_multiplier(params[CONF_KI_MULTIPLIER]))
        cg.add(var.set_kd_multiplier(params[CONF_KD_MULTIPLIER]))
