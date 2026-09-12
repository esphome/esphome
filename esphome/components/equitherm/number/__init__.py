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
    ICON_THERMOMETER,
    UNIT_CELSIUS,
)

from ..climate import EquithermClimate, equitherm_ns

# =============================================================================
# C++ Class Declarations
# =============================================================================

FallbackOutdoorTempNumber = equitherm_ns.class_(
    "FallbackOutdoorTempNumber", number.Number, cg.Component
)
RateLimitRisingNumber = equitherm_ns.class_(
    "RateLimitRisingNumber", number.Number, cg.Component
)
RateLimitFallingNumber = equitherm_ns.class_(
    "RateLimitFallingNumber", number.Number, cg.Component
)

# Fallback
CONF_FALLBACK_OUTDOOR_TEMP = "fallback_outdoor_temp"

# Output rate limiting (asymmetric)
CONF_RATE_LIMIT_RISING = "rate_limit_rising"
CONF_RATE_LIMIT_FALLING = "rate_limit_falling"

# =============================================================================
# Default Values
# =============================================================================

# Fallback defaults (wide range for different climate strategies)
DEFAULT_FALLBACK_MIN, DEFAULT_FALLBACK_MAX, DEFAULT_FALLBACK_STEP = -40.0, 30.0, 0.5

# Rate limit defaults
DEFAULT_RATE_LIMIT_MIN, DEFAULT_RATE_LIMIT_MAX, DEFAULT_RATE_LIMIT_STEP = 0.0, 2.0, 0.1

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

FALLBACK_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FALLBACK_OUTDOOR_TEMP): _number_schema(
            FallbackOutdoorTempNumber,
            icon=ICON_THERMOMETER,
            device_class=DEVICE_CLASS_TEMPERATURE,
            unit=UNIT_CELSIUS,
        ),
    }
)

RATE_LIMIT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RATE_LIMIT_RISING): _number_schema(
            RateLimitRisingNumber,
            icon="mdi:thermometer-chevron-up",
            unit="°C/min",
        ),
        cv.Optional(CONF_RATE_LIMIT_FALLING): _number_schema(
            RateLimitFallingNumber,
            icon="mdi:thermometer-chevron-down",
            unit="°C/min",
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
    .extend(FALLBACK_SCHEMA)
    .extend(RATE_LIMIT_SCHEMA)
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


async def _register_fallback_numbers(config, parent_id):
    """Register fallback number entities."""
    if fallback_config := config.get(CONF_FALLBACK_OUTDOOR_TEMP):
        await _register_number(
            fallback_config,
            parent_id,
            DEFAULT_FALLBACK_MIN,
            DEFAULT_FALLBACK_MAX,
            DEFAULT_FALLBACK_STEP,
        )


async def _register_rate_limit_numbers(config, parent_id):
    """Register rate limit number entities."""
    if rate_limit_rising_config := config.get(CONF_RATE_LIMIT_RISING):
        await _register_number(
            rate_limit_rising_config,
            parent_id,
            DEFAULT_RATE_LIMIT_MIN,
            DEFAULT_RATE_LIMIT_MAX,
            DEFAULT_RATE_LIMIT_STEP,
        )

    if rate_limit_falling_config := config.get(CONF_RATE_LIMIT_FALLING):
        await _register_number(
            rate_limit_falling_config,
            parent_id,
            DEFAULT_RATE_LIMIT_MIN,
            DEFAULT_RATE_LIMIT_MAX,
            DEFAULT_RATE_LIMIT_STEP,
        )


# =============================================================================
# Code Generation Entry Point
# =============================================================================


async def to_code(config):
    """Generate C++ code for all equitherm number entities."""
    parent_id = config[CONF_CLIMATE_ID]

    await _register_fallback_numbers(config, parent_id)
    await _register_rate_limit_numbers(config, parent_id)
