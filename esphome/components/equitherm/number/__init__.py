import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    ENTITY_CATEGORY_CONFIG,
    ICON_GAUGE,
    ICON_THERMOMETER,
)

from ..climate import EquithermClimate, equitherm_ns

# Explicit number classes for each parameter type
HeatCurveCoefficientNumber = equitherm_ns.class_(
    "HeatCurveCoefficientNumber", number.Number, cg.Component
)
HeatCurveExponentNumber = equitherm_ns.class_(
    "HeatCurveExponentNumber", number.Number, cg.Component
)
HeatCurveShiftNumber = equitherm_ns.class_(
    "HeatCurveShiftNumber", number.Number, cg.Component
)
PIDProportionalGainNumber = equitherm_ns.class_(
    "PIDProportionalGainNumber", number.Number, cg.Component
)
PIDIntegralGainNumber = equitherm_ns.class_(
    "PIDIntegralGainNumber", number.Number, cg.Component
)
PIDDerivativeGainNumber = equitherm_ns.class_(
    "PIDDerivativeGainNumber", number.Number, cg.Component
)
FallbackOutdoorTempNumber = equitherm_ns.class_(
    "FallbackOutdoorTempNumber", number.Number, cg.Component
)

CONF_CLIMATE_ID = "climate_id"

# Configuration keys for each number type
CONF_HEAT_CURVE_COEFFICIENT = "heat_curve_coefficient"
CONF_HEAT_CURVE_EXPONENT = "heat_curve_exponent"
CONF_HEAT_CURVE_SHIFT = "heat_curve_shift"
CONF_PID_PROPORTIONAL_GAIN = "pid_proportional_gain"
CONF_PID_INTEGRAL_GAIN = "pid_integral_gain"
CONF_PID_DERIVATIVE_GAIN = "pid_derivative_gain"
CONF_FALLBACK_OUTDOOR_TEMP = "fallback_outdoor_temp"


def _number_schema(number_class, icon=ICON_GAUGE):
    """Generate schema for a specific number type with min/max/step support."""
    return number.number_schema(
        number_class,
        icon=icon,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ).extend(
        {
            cv.Optional(CONF_MIN_VALUE): cv.float_,
            cv.Optional(CONF_MAX_VALUE): cv.float_,
            cv.Optional(CONF_STEP): cv.positive_float,
        }
    )


def _fallback_number_schema(number_class):
    """Generate schema for fallback number with thermometer icon."""
    return number.number_schema(
        number_class,
        icon=ICON_THERMOMETER,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ).extend(
        {
            cv.Optional(CONF_MIN_VALUE): cv.float_,
            cv.Optional(CONF_MAX_VALUE): cv.float_,
            cv.Optional(CONF_STEP): cv.positive_float,
        }
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(EquithermClimate),
        cv.Optional(CONF_HEAT_CURVE_COEFFICIENT): _number_schema(
            HeatCurveCoefficientNumber
        ),
        cv.Optional(CONF_HEAT_CURVE_EXPONENT): _number_schema(HeatCurveExponentNumber),
        cv.Optional(CONF_HEAT_CURVE_SHIFT): _number_schema(HeatCurveShiftNumber),
        cv.Optional(CONF_PID_PROPORTIONAL_GAIN): _number_schema(
            PIDProportionalGainNumber
        ),
        cv.Optional(CONF_PID_INTEGRAL_GAIN): _number_schema(PIDIntegralGainNumber),
        cv.Optional(CONF_PID_DERIVATIVE_GAIN): _number_schema(PIDDerivativeGainNumber),
        cv.Optional(CONF_FALLBACK_OUTDOOR_TEMP): _fallback_number_schema(
            FallbackOutdoorTempNumber
        ),
    }
)


async def _register_number(config, parent_id, default_min, default_max, default_step):
    """Helper to register a number entity with defaults for min/max/step."""
    min_val = config.get(CONF_MIN_VALUE, default_min)
    max_val = config.get(CONF_MAX_VALUE, default_max)
    step = config.get(CONF_STEP, default_step)

    n = await number.new_number(config, min_value=min_val, max_value=max_val, step=step)
    await cg.register_component(n, config)
    await cg.register_parented(n, parent_id)
    return n


async def to_code(config):
    parent_id = config[CONF_CLIMATE_ID]

    if hc_config := config.get(CONF_HEAT_CURVE_COEFFICIENT):
        await _register_number(hc_config, parent_id, 0.1, 10.0, 0.1)

    if n_config := config.get(CONF_HEAT_CURVE_EXPONENT):
        await _register_number(n_config, parent_id, 0.5, 3.0, 0.05)

    if shift_config := config.get(CONF_HEAT_CURVE_SHIFT):
        await _register_number(shift_config, parent_id, -20.0, 20.0, 0.5)

    if kp_config := config.get(CONF_PID_PROPORTIONAL_GAIN):
        await _register_number(kp_config, parent_id, 0.0, 20.0, 0.1)

    if ki_config := config.get(CONF_PID_INTEGRAL_GAIN):
        await _register_number(ki_config, parent_id, 0.0, 10.0, 0.01)

    if kd_config := config.get(CONF_PID_DERIVATIVE_GAIN):
        await _register_number(kd_config, parent_id, 0.0, 10.0, 0.1)

    if fallback_config := config.get(CONF_FALLBACK_OUTDOOR_TEMP):
        await _register_number(fallback_config, parent_id, -20.0, 15.0, 1.0)
