import esphome.codegen as cg
from esphome.components import climate, number, output, sensor
import esphome.config_validation as cv

CODEOWNERS = ["@P4uLT"]

CONF_DEFAULT_TARGET_TEMPERATURE = "default_target_temperature"
CONF_OUTDOOR_SENSOR = "outdoor_sensor"
CONF_INDOOR_SENSOR = "indoor_sensor"
CONF_CH_SETPOINT = "ch_setpoint"
CONF_HEAT_OUTPUT = "heat_output"
CONF_CONTROL_PARAMETERS = "control_parameters"
CONF_OUTPUT_PARAMETERS = "output_parameters"
CONF_DEADBAND_PARAMETERS = "deadband_parameters"
CONF_SLOPE = "slope"
CONF_EXPONENT = "exponent"
CONF_SHIFT = "shift"
CONF_T_MIN_FLOW = "t_min_flow"
CONF_T_MAX_FLOW = "t_max_flow"
CONF_TARGET_DIFF_FACTOR = "target_diff_factor"  # Phase B: Room correction
CONF_ROOM_ERROR_CLAMP = "room_error_clamp"  # Max room error correction
CONF_SMOOTHING_THRESHOLD = "smoothing_threshold"  # Minimum change to trigger output

# PID parameters (Phase D)
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

equitherm_climate_ns = cg.esphome_ns.namespace("equitherm_climate")
EquithermClimate = equitherm_climate_ns.class_(
    "EquithermClimate", climate.Climate, cg.Component
)

CONTROL_PARAMETERS_SCHEMA = cv.Schema(
    {
        # Equitherm curve parameters
        cv.Required(CONF_SLOPE): cv.float_range(
            min=0.1
        ),  # Minimum 0.1 for numerical stability
        cv.Optional(CONF_EXPONENT, default=1.5): cv.positive_float,
        cv.Optional(CONF_SHIFT, default=0.0): cv.float_,
        cv.Optional(CONF_TARGET_DIFF_FACTOR, default=1.0): cv.float_range(min=0.0),
        cv.Optional(CONF_ROOM_ERROR_CLAMP, default=3.0): cv.float_range(min=0.0),
        # PID trim parameters (Phase D)
        cv.Optional(CONF_KP, default=0.0): cv.float_,
        cv.Optional(CONF_KI, default=0.0): cv.float_,
        cv.Optional(CONF_KD, default=0.0): cv.float_,
        cv.Optional(CONF_MIN_INTEGRAL, default=-10.0): cv.float_,
        cv.Optional(CONF_MAX_INTEGRAL, default=10.0): cv.float_,
    }
)

OUTPUT_PARAMETERS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_T_MIN_FLOW, default=25.0): cv.temperature,
        cv.Optional(CONF_T_MAX_FLOW, default=70.0): cv.temperature,
        cv.Optional(CONF_SMOOTHING_THRESHOLD, default=0.5): cv.float_range(min=0.0),
    }
)

DEADBAND_PARAMETERS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_THRESHOLD_HIGH, default=0.0): cv.float_range(min=0.0),
        cv.Optional(CONF_THRESHOLD_LOW, default=0.0): cv.float_range(min=0.0),
        cv.Optional(CONF_KP_MULTIPLIER, default=0.0): cv.float_range(min=0.0),
        cv.Optional(CONF_KI_MULTIPLIER, default=0.0): cv.float_range(min=0.0),
    }
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

    # Control parameters (equitherm curve + PID)
    params = config[CONF_CONTROL_PARAMETERS]
    cg.add(var.set_slope(params[CONF_SLOPE]))
    cg.add(var.set_exponent(params[CONF_EXPONENT]))
    cg.add(var.set_shift(params[CONF_SHIFT]))
    cg.add(var.set_target_diff_factor(params[CONF_TARGET_DIFF_FACTOR]))
    cg.add(var.set_room_error_clamp(params[CONF_ROOM_ERROR_CLAMP]))
    # PID parameters
    cg.add(var.set_kp(params[CONF_KP]))
    cg.add(var.set_ki(params[CONF_KI]))
    cg.add(var.set_kd(params[CONF_KD]))
    cg.add(var.set_min_integral(params[CONF_MIN_INTEGRAL]))
    cg.add(var.set_max_integral(params[CONF_MAX_INTEGRAL]))

    # Output parameters
    params = config[CONF_OUTPUT_PARAMETERS]
    cg.add(var.set_t_min_flow(params[CONF_T_MIN_FLOW]))
    cg.add(var.set_t_max_flow(params[CONF_T_MAX_FLOW]))
    cg.add(var.set_smoothing_threshold(params[CONF_SMOOTHING_THRESHOLD]))

    # Deadband parameters - optional
    if CONF_DEADBAND_PARAMETERS in config:
        params = config[CONF_DEADBAND_PARAMETERS]
        cg.add(var.set_threshold_high(params[CONF_THRESHOLD_HIGH]))
        cg.add(var.set_threshold_low(params[CONF_THRESHOLD_LOW]))
        cg.add(var.set_kp_multiplier(params[CONF_KP_MULTIPLIER]))
        cg.add(var.set_ki_multiplier(params[CONF_KI_MULTIPLIER]))
