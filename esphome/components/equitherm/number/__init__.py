import esphome.codegen as cg
from esphome.components import number
from esphome.components.const import CONF_CLIMATE_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    CONF_RESTORE_VALUE,
    CONF_STEP,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_CELSIUS,
)

from ..climate import EquithermClimate, equitherm_ns

# =============================================================================
# C++ Class Declarations
# =============================================================================

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

# Heating curve parameters
CONF_HEAT_CURVE_COEFFICIENT = "heat_curve_coefficient"
CONF_HEAT_CURVE_EXPONENT = "heat_curve_exponent"
CONF_HEAT_CURVE_SHIFT = "heat_curve_shift"

# PID tuning parameters
CONF_PID_PROPORTIONAL_GAIN = "pid_proportional_gain"
CONF_PID_INTEGRAL_GAIN = "pid_integral_gain"
CONF_PID_DERIVATIVE_GAIN = "pid_derivative_gain"

# =============================================================================
# Icons (component-specific, not in esphome/const.py)
# =============================================================================

# Heating curve icons - each represents a specific aspect of the heating curve
CONF_ICON_HEATING_CURVE_SLOPE = (
    "mdi:slope-uphill"  # Coefficient - controls curve steepness
)
CONF_ICON_HEATING_CURVE_CURVATURE = (
    "mdi:tune-variant"  # Exponent - controls curve shape
)
CONF_ICON_HEATING_CURVE_OFFSET = "mdi:arrow-up-down"  # Shift - parallel displacement
CONF_ICON_PID_TUNING = "mdi:tune"  # PID gains - tuning parameters

# =============================================================================
# Default Values
# =============================================================================

# Heating curve defaults
DEFAULT_HC_MIN, DEFAULT_HC_MAX, DEFAULT_HC_STEP = 0.5, 5.0, 0.05
DEFAULT_N_MIN, DEFAULT_N_MAX, DEFAULT_N_STEP = 0.5, 3.0, 0.05
DEFAULT_SHIFT_MIN, DEFAULT_SHIFT_MAX, DEFAULT_SHIFT_STEP = -20.0, 20.0, 0.5

# PID defaults
DEFAULT_KP_MIN, DEFAULT_KP_MAX, DEFAULT_KP_STEP = 0.0, 20.0, 0.01
DEFAULT_KI_MIN, DEFAULT_KI_MAX, DEFAULT_KI_STEP = 0.0, 10.0, 0.0001
DEFAULT_KD_MIN, DEFAULT_KD_MAX, DEFAULT_KD_STEP = 0.0, 10.0, 0.01

# =============================================================================
# Validation
# =============================================================================


def _validate_min_max(config):
    """Validate that max_value > min_value if both are specified."""
    min_val = config.get(CONF_MIN_VALUE)
    max_val = config.get(CONF_MAX_VALUE)
    if min_val is not None and max_val is not None and max_val <= min_val:
        raise cv.Invalid("max_value must be greater than min_value")
    return config


# =============================================================================
# Schema Helpers
# =============================================================================


def _number_schema(number_class, icon, device_class=None, unit=None):
    """Generate schema for a number with sensible defaults and full customization.

    These numbers are runtime tuning interfaces for parameters defined in the climate
    component's YAML config. The initial value comes from the climate config, not from
    an initial_value option here. restore_value controls whether user tuning persists.

    Args:
        number_class: The C++ number class to instantiate
        icon: Default icon (can be overridden by user)
        device_class: Optional device class for semantic meaning
        unit: Optional unit of measurement

    Returns:
        Schema that extends number_schema with min/max/step/mode/restore_value
    """
    kwargs = {
        "icon": icon,
        "entity_category": ENTITY_CATEGORY_CONFIG,
    }
    if device_class:
        kwargs["device_class"] = device_class
    if unit:
        kwargs["unit_of_measurement"] = unit

    return cv.All(
        number.number_schema(number_class, **kwargs).extend(
            {
                cv.Optional(CONF_MIN_VALUE): cv.float_,
                cv.Optional(CONF_MAX_VALUE): cv.float_,
                cv.Optional(CONF_STEP): cv.positive_float,
                cv.Optional(CONF_MODE, default="AUTO"): cv.enum(
                    number.NUMBER_MODES, upper=True
                ),
                cv.Optional(CONF_RESTORE_VALUE, default=True): cv.boolean,
            }
        ),
        _validate_min_max,
    )


# =============================================================================
# Grouped Schemas
# =============================================================================

HEATING_CURVE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_HEAT_CURVE_COEFFICIENT): _number_schema(
            HeatCurveCoefficientNumber, icon=CONF_ICON_HEATING_CURVE_SLOPE
        ),
        cv.Optional(CONF_HEAT_CURVE_EXPONENT): _number_schema(
            HeatCurveExponentNumber, icon=CONF_ICON_HEATING_CURVE_CURVATURE
        ),
        cv.Optional(CONF_HEAT_CURVE_SHIFT): _number_schema(
            HeatCurveShiftNumber,
            icon=CONF_ICON_HEATING_CURVE_OFFSET,
            device_class=DEVICE_CLASS_TEMPERATURE,
            unit=UNIT_CELSIUS,
        ),
    }
)

PID_GAINS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PID_PROPORTIONAL_GAIN): _number_schema(
            PIDProportionalGainNumber, icon=CONF_ICON_PID_TUNING
        ),
        cv.Optional(CONF_PID_INTEGRAL_GAIN): _number_schema(
            PIDIntegralGainNumber, icon=CONF_ICON_PID_TUNING
        ),
        cv.Optional(CONF_PID_DERIVATIVE_GAIN): _number_schema(
            PIDDerivativeGainNumber, icon=CONF_ICON_PID_TUNING
        ),
    }
)

# =============================================================================
# Main Configuration Schema
# =============================================================================

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
            cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(EquithermClimate),
        }
    )
    .extend(HEATING_CURVE_SCHEMA)
    .extend(PID_GAINS_SCHEMA)
)

# =============================================================================
# Code Generation Helpers
# =============================================================================


async def _register_number(config, parent_id, default_min, default_max, default_step):
    """Register a number entity with defaults for min/max/step.

    The initial value is read from the parent climate component in C++ setup(),
    so we don't set an initial_value here. restore_value controls persistence.
    """
    min_val = config.get(CONF_MIN_VALUE, default_min)
    max_val = config.get(CONF_MAX_VALUE, default_max)
    step = config.get(CONF_STEP, default_step)
    restore_val = config.get(CONF_RESTORE_VALUE, True)

    n = await number.new_number(config, min_value=min_val, max_value=max_val, step=step)
    await cg.register_component(n, config)
    await cg.register_parented(n, parent_id)
    cg.add(n.set_restore_value(restore_val))

    return n


async def _register_heating_curve_numbers(config, parent_id):
    """Register all heating curve number entities."""
    if hc_config := config.get(CONF_HEAT_CURVE_COEFFICIENT):
        await _register_number(
            hc_config, parent_id, DEFAULT_HC_MIN, DEFAULT_HC_MAX, DEFAULT_HC_STEP
        )

    if n_config := config.get(CONF_HEAT_CURVE_EXPONENT):
        await _register_number(
            n_config, parent_id, DEFAULT_N_MIN, DEFAULT_N_MAX, DEFAULT_N_STEP
        )

    if shift_config := config.get(CONF_HEAT_CURVE_SHIFT):
        await _register_number(
            shift_config,
            parent_id,
            DEFAULT_SHIFT_MIN,
            DEFAULT_SHIFT_MAX,
            DEFAULT_SHIFT_STEP,
        )


async def _register_pid_numbers(config, parent_id):
    """Register all PID gain number entities."""
    if kp_config := config.get(CONF_PID_PROPORTIONAL_GAIN):
        await _register_number(
            kp_config, parent_id, DEFAULT_KP_MIN, DEFAULT_KP_MAX, DEFAULT_KP_STEP
        )

    if ki_config := config.get(CONF_PID_INTEGRAL_GAIN):
        await _register_number(
            ki_config, parent_id, DEFAULT_KI_MIN, DEFAULT_KI_MAX, DEFAULT_KI_STEP
        )

    if kd_config := config.get(CONF_PID_DERIVATIVE_GAIN):
        await _register_number(
            kd_config, parent_id, DEFAULT_KD_MIN, DEFAULT_KD_MAX, DEFAULT_KD_STEP
        )


# =============================================================================
# Code Generation Entry Point
# =============================================================================


async def to_code(config):
    """Generate C++ code for all equitherm number entities."""
    parent_id = config[CONF_CLIMATE_ID]

    await _register_heating_curve_numbers(config, parent_id)
    await _register_pid_numbers(config, parent_id)
