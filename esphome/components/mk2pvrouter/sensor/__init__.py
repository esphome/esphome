import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ID,
    CONF_STATE_CLASS,
    CONF_TAG,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)
from esphome.types import ConfigType

from .. import (
    CONF_MK2PVROUTER_ID,
    MK2PVROUTER_LISTENER_SCHEMA,
    mk2pvrouter_ns,
    register_mk2pvrouter_listener,
)

Mk2PVRouterSensor = mk2pvrouter_ns.class_(
    "Mk2PVRouterSensor", sensor.Sensor, cg.Component
)

_DEFAULT_VALIDATORS = {
    CONF_STATE_CLASS: sensor.validate_state_class,
    CONF_DEVICE_CLASS: sensor.validate_device_class,
    CONF_UNIT_OF_MEASUREMENT: sensor.validate_unit_of_measurement,
}

# Define common sensor configurations to avoid repetition
POWER_CONFIG = {
    CONF_UNIT_OF_MEASUREMENT: UNIT_WATT,
    CONF_DEVICE_CLASS: DEVICE_CLASS_POWER,
    CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
    CONF_ACCURACY_DECIMALS: 0,
}

VOLTAGE_CONFIG = {
    CONF_UNIT_OF_MEASUREMENT: UNIT_VOLT,
    CONF_DEVICE_CLASS: DEVICE_CLASS_VOLTAGE,
    CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
    CONF_ACCURACY_DECIMALS: 2,
    # Device sends voltage * 100; corrected in Mk2PVRouterSensor::publish_val().
}

ENERGY_CONFIG = {
    CONF_UNIT_OF_MEASUREMENT: UNIT_WATT_HOURS,
    CONF_DEVICE_CLASS: DEVICE_CLASS_ENERGY,
    CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
    CONF_ACCURACY_DECIMALS: 0,
}

TEMPERATURE_CONFIG = {
    CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
    CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
    CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
    CONF_ACCURACY_DECIMALS: 2,
    # Device sends temperature * 100; corrected in Mk2PVRouterSensor::publish_val().
}

RELAY_STATE_CONFIG = {
    CONF_UNIT_OF_MEASUREMENT: UNIT_EMPTY,
    CONF_DEVICE_CLASS: DEVICE_CLASS_EMPTY,
    CONF_STATE_CLASS: STATE_CLASS_NONE,
}

DIVERSION_RATE_CONFIG = {
    CONF_UNIT_OF_MEASUREMENT: UNIT_PERCENT,
    CONF_DEVICE_CLASS: DEVICE_CLASS_EMPTY,
    CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
}

# Per the Mk2PVRouter firmware protocol (FredM67/PVRouter-1-phase,
# FredM67/PVRouter-3-phase), some letters carry a different meaning
# depending on whether they're sent bare or with a trailing index:
#   D  bare = diverted power (W);      D1, D2, ... = diversion rate per load (%)
#   R  bare = mean relay power (W);    R1, R2, ... = relay state (0/1)
# T is always indexed (T1, T2, ...); E is never indexed (no E1/E2/E3).
SENSOR_CONFIGS = {
    # Pattern configs for tags with numbers (e.g., P1, P2, V1, V2)
    "patterns": {
        "P": POWER_CONFIG,  # Power readings (P1, P2, P3)
        "D": DIVERSION_RATE_CONFIG,  # Diversion rate (D1, D2, etc.)
        "V": VOLTAGE_CONFIG,  # Voltage readings (V1, V2, V3)
        "T": TEMPERATURE_CONFIG,  # Temperature readings (T1, T2, etc.)
        "R": RELAY_STATE_CONFIG,  # Relay states (R1, R2, etc.)
    },
    # Exact tag configs for single-letter or special tags
    "exact": {
        "P": POWER_CONFIG,  # Total power (single- and three-phase)
        "D": POWER_CONFIG,  # Diverted power (single-phase, W)
        "V": VOLTAGE_CONFIG,  # Voltage (single-phase)
        "E": ENERGY_CONFIG,  # Diverted energy (never indexed)
        "R": POWER_CONFIG,  # Mean power for relay diversion (single- and three-phase)
    },
}


# Create a base schema that's flexible for any tag
# Note: Don't set defaults here for values that vary by tag type
# (accuracy_decimals, state_class) - let tag-specific configs set them
BASE_SCHEMA = sensor.sensor_schema(
    Mk2PVRouterSensor,
).extend(MK2PVROUTER_LISTENER_SCHEMA)


def _apply_defaults(config: ConfigType, defaults: dict) -> None:
    """Inject defaults into config, skipping keys already set by the user.

    Values are run through the same validators sensor_schema() would use, so
    they are code-generation-ready and a typo'd constant fails validation
    instead of shipping silently.
    """
    for key, value in defaults.items():
        if key not in config:
            if key in _DEFAULT_VALIDATORS:
                value = _DEFAULT_VALIDATORS[key](value)
            config[key] = value


def _resolve_tag_defaults(tag: str) -> dict | None:
    """Resolve the defaults dict for a tag, by exact match then pattern match."""
    tag_upper = tag.upper()

    # First, check for exact match (single letter or special tags)
    if tag_upper in SENSOR_CONFIGS["exact"]:
        return SENSOR_CONFIGS["exact"][tag_upper]

    # Then check for pattern match (letter + number)
    if len(tag_upper) >= 2:
        prefix = tag_upper[0]
        suffix = tag_upper[1:]
        # Check if it matches pattern: letter followed by digits
        if prefix in SENSOR_CONFIGS["patterns"] and suffix.isdigit():
            return SENSOR_CONFIGS["patterns"][prefix]

    return None


def is_centi_scaled(tag: str) -> bool:
    """Whether the device sends this tag multiplied by 100 (centivolts/centi-degrees)."""
    defaults = _resolve_tag_defaults(tag)
    return defaults is VOLTAGE_CONFIG or defaults is TEMPERATURE_CONFIG


def apply_tag_defaults(config: ConfigType) -> ConfigType:
    """Apply defaults based on tag pattern or exact match."""
    defaults = _resolve_tag_defaults(config[CONF_TAG])

    if defaults:
        _apply_defaults(config, defaults)

    # Fallback: ensure defaults for unknown tags
    _apply_defaults(
        config,
        {
            CONF_ACCURACY_DECIMALS: 0,
            CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        },
    )

    return config


CONFIG_SCHEMA = cv.All(BASE_SCHEMA, apply_tag_defaults)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(
        config[CONF_ID], config[CONF_TAG], is_centi_scaled(config[CONF_TAG])
    )
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    mk2pvrouter = await cg.get_variable(config[CONF_MK2PVROUTER_ID])
    await register_mk2pvrouter_listener(mk2pvrouter, var)
