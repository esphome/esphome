from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, output, sensor
import esphome.config_validation as cv
from esphome.const import CONF_HUMIDITY_SENSOR, CONF_ID, CONF_SENSOR
from esphome.core import ID, Lambda
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

pid_ns = cg.esphome_ns.namespace("pid")
PIDClimate = pid_ns.class_("PIDClimate", climate.Climate, cg.Component)
PIDAutotuneAction = pid_ns.class_("PIDAutotuneAction", automation.Action)

CONF_DEFAULT_TARGET_TEMPERATURE = "default_target_temperature"

CONF_KP = "kp"
CONF_KI = "ki"
CONF_STARTING_INTEGRAL_TERM = "starting_integral_term"
CONF_KD = "kd"
CONF_CONTROL_PARAMETERS = "control_parameters"
CONF_COOL_OUTPUT = "cool_output"
CONF_HEAT_OUTPUT = "heat_output"
CONF_NOISEBAND = "noiseband"
CONF_POSITIVE_OUTPUT = "positive_output"
CONF_NEGATIVE_OUTPUT = "negative_output"
CONF_MIN_INTEGRAL = "min_integral"
CONF_MAX_INTEGRAL = "max_integral"
CONF_OUTPUT_AVERAGING_SAMPLES = "output_averaging_samples"
CONF_DERIVATIVE_AVERAGING_SAMPLES = "derivative_averaging_samples"

# Deadband parameters
CONF_DEADBAND_PARAMETERS = "deadband_parameters"
CONF_THRESHOLD_HIGH = "threshold_high"
CONF_THRESHOLD_LOW = "threshold_low"
CONF_DEADBAND_OUTPUT_AVERAGING_SAMPLES = "deadband_output_averaging_samples"
CONF_KP_MULTIPLIER = "kp_multiplier"
CONF_KI_MULTIPLIER = "ki_multiplier"
CONF_KD_MULTIPLIER = "kd_multiplier"


def _validate_thresholds(config: ConfigType) -> ConfigType:
    # Same rule as PIDClimate::set_deadband_thresholds; equal is allowed since 0/0 is the default.
    if config[CONF_THRESHOLD_LOW] > config[CONF_THRESHOLD_HIGH]:
        raise cv.Invalid(
            f"{CONF_THRESHOLD_LOW} must not be greater than {CONF_THRESHOLD_HIGH}"
        )
    return config


def _validate_threshold_action(config: ConfigType) -> ConfigType:
    threshold_low = config[CONF_THRESHOLD_LOW]
    threshold_high = config[CONF_THRESHOLD_HIGH]
    if isinstance(threshold_low, Lambda) or isinstance(threshold_high, Lambda):
        return config
    return _validate_thresholds(config)


CONFIG_SCHEMA = cv.All(
    climate.climate_schema(PIDClimate).extend(
        {
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HUMIDITY_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_DEFAULT_TARGET_TEMPERATURE): cv.temperature,
            cv.Optional(CONF_COOL_OUTPUT): cv.use_id(output.FloatOutput),
            cv.Optional(CONF_HEAT_OUTPUT): cv.use_id(output.FloatOutput),
            cv.Optional(CONF_DEADBAND_PARAMETERS): cv.All(
                {
                    cv.Required(CONF_THRESHOLD_HIGH): cv.temperature_delta,
                    cv.Required(CONF_THRESHOLD_LOW): cv.temperature_delta,
                    cv.Optional(CONF_KP_MULTIPLIER, default=0.1): cv.float_,
                    cv.Optional(CONF_KI_MULTIPLIER, default=0.0): cv.float_,
                    cv.Optional(CONF_KD_MULTIPLIER, default=0.0): cv.float_,
                    cv.Optional(
                        CONF_DEADBAND_OUTPUT_AVERAGING_SAMPLES, default=1
                    ): cv.positive_not_null_int,
                },
                _validate_thresholds,
            ),
            cv.Required(CONF_CONTROL_PARAMETERS): cv.Schema(
                {
                    cv.Required(CONF_KP): cv.float_,
                    cv.Optional(CONF_KI, default=0.0): cv.float_,
                    cv.Optional(CONF_KD, default=0.0): cv.float_,
                    cv.Optional(CONF_STARTING_INTEGRAL_TERM, default=0.0): cv.float_,
                    cv.Optional(CONF_MIN_INTEGRAL, default=-1): cv.float_,
                    cv.Optional(CONF_MAX_INTEGRAL, default=1): cv.float_,
                    cv.Optional(
                        CONF_DERIVATIVE_AVERAGING_SAMPLES, default=1
                    ): cv.positive_not_null_int,
                    cv.Optional(
                        CONF_OUTPUT_AVERAGING_SAMPLES, default=1
                    ): cv.positive_not_null_int,
                }
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_COOL_OUTPUT, CONF_HEAT_OUTPUT),
)


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))

    if CONF_HUMIDITY_SENSOR in config:
        sens = await cg.get_variable(config[CONF_HUMIDITY_SENSOR])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_COOL_OUTPUT in config:
        out = await cg.get_variable(config[CONF_COOL_OUTPUT])
        cg.add(var.set_cool_output(out))
    if CONF_HEAT_OUTPUT in config:
        out = await cg.get_variable(config[CONF_HEAT_OUTPUT])
        cg.add(var.set_heat_output(out))
    params = config[CONF_CONTROL_PARAMETERS]
    cg.add(var.set_kp(params[CONF_KP]))
    cg.add(var.set_ki(params[CONF_KI]))
    cg.add(var.set_kd(params[CONF_KD]))
    cg.add(var.set_starting_integral_term(params[CONF_STARTING_INTEGRAL_TERM]))
    cg.add(var.set_derivative_samples(params[CONF_DERIVATIVE_AVERAGING_SAMPLES]))

    output_samples = params[CONF_OUTPUT_AVERAGING_SAMPLES]
    cg.add(var.set_output_samples(output_samples))

    if CONF_MIN_INTEGRAL in params:
        cg.add(var.set_min_integral(params[CONF_MIN_INTEGRAL]))
    if CONF_MAX_INTEGRAL in params:
        cg.add(var.set_max_integral(params[CONF_MAX_INTEGRAL]))

    deadband_output_samples = 1
    if CONF_DEADBAND_PARAMETERS in config:
        params = config[CONF_DEADBAND_PARAMETERS]
        cg.add(var.set_threshold_low(params[CONF_THRESHOLD_LOW]))
        cg.add(var.set_threshold_high(params[CONF_THRESHOLD_HIGH]))
        cg.add(var.set_kp_multiplier(params[CONF_KP_MULTIPLIER]))
        cg.add(var.set_ki_multiplier(params[CONF_KI_MULTIPLIER]))
        cg.add(var.set_kd_multiplier(params[CONF_KD_MULTIPLIER]))
        deadband_output_samples = params[CONF_DEADBAND_OUTPUT_AVERAGING_SAMPLES]
        cg.add(var.set_deadband_output_samples(deadband_output_samples))

    # Single shared output buffer sized to max of both modes
    cg.add(var.init_output_buffer(max(output_samples, deadband_output_samples)))

    cg.add(var.set_default_target_temperature(config[CONF_DEFAULT_TARGET_TEMPERATURE]))


automation.register_apply_action(
    "climate.pid.reset_integral_term",
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(PIDClimate),
        }
    ),
    automation.ApplyCall("reset_integral_term()"),
)


@automation.register_action(
    "climate.pid.autotune",
    PIDAutotuneAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(PIDClimate),
            cv.Optional(CONF_NOISEBAND, default=0.25): cv.float_,
            cv.Optional(
                CONF_POSITIVE_OUTPUT, default=1.0
            ): cv.possibly_negative_percentage,
            cv.Optional(
                CONF_NEGATIVE_OUTPUT, default=-1.0
            ): cv.possibly_negative_percentage,
        }
    ),
    synchronous=True,
)
async def esp8266_set_frequency_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_noiseband(config[CONF_NOISEBAND]))
    cg.add(var.set_positive_output(config[CONF_POSITIVE_OUTPUT]))
    cg.add(var.set_negative_output(config[CONF_NEGATIVE_OUTPUT]))
    return var


automation.register_apply_action(
    "climate.pid.set_control_parameters",
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(PIDClimate),
            cv.Required(CONF_KP): cv.templatable(cv.float_),
            cv.Optional(CONF_KI, default=0.0): cv.templatable(cv.float_),
            cv.Optional(CONF_KD, default=0.0): cv.templatable(cv.float_),
        }
    ),
    automation.ApplyField(CONF_KP, "set_kp", cg.float_),
    automation.ApplyField(CONF_KI, "set_ki", cg.float_),
    automation.ApplyField(CONF_KD, "set_kd", cg.float_),
)


automation.register_apply_action(
    "climate.pid.set_deadband_control_parameters_multipliers",
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(PIDClimate),
            # kp_multiplier is required for compatibility with the original action API;
            # ki_multiplier and kd_multiplier are optional overrides.
            cv.Required(CONF_KP_MULTIPLIER): cv.templatable(cv.float_),
            cv.Optional(CONF_KI_MULTIPLIER): cv.templatable(cv.float_),
            cv.Optional(CONF_KD_MULTIPLIER): cv.templatable(cv.float_),
        }
    ),
    automation.ApplyField(CONF_KP_MULTIPLIER, "set_kp_multiplier", cg.float_),
    automation.ApplyField(CONF_KI_MULTIPLIER, "set_ki_multiplier", cg.float_),
    automation.ApplyField(CONF_KD_MULTIPLIER, "set_kd_multiplier", cg.float_),
)


automation.register_apply_action(
    "climate.pid.set_deadband_threshold_parameters",
    automation.maybe_simple_id(
        cv.All(
            {
                cv.Required(CONF_ID): cv.use_id(PIDClimate),
                cv.Required(CONF_THRESHOLD_HIGH): cv.templatable(cv.temperature_delta),
                cv.Required(CONF_THRESHOLD_LOW): cv.templatable(cv.temperature_delta),
            },
            _validate_threshold_action,
        )
    ),
    automation.ApplyCall(
        "set_deadband_thresholds({}, {})",
        ((CONF_THRESHOLD_LOW, cg.float_), (CONF_THRESHOLD_HIGH, cg.float_)),
    ),
)
