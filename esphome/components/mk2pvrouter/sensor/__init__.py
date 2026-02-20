import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_FILTERS,
    CONF_ID,
    CONF_MULTIPLY,
    CONF_STATE_CLASS,
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

from .. import (
    CONF_MK2PVROUTER_ID,
    CONF_TAG_NAME,
    MK2PVROUTER_LISTENER_SCHEMA,
    mk2pvrouter_ns,
    register_listener,
)

Mk2PVRouterSensor = mk2pvrouter_ns.class_(
    "Mk2PVRouterSensor", sensor.Sensor, cg.Component
)

# Build validators from sensor module's public data structures (STATE_CLASSES,
# FILTER_REGISTRY) via cv public API, rather than importing the sensor module's
# private _validate_state_class/_validate_filters helper functions directly.
_validate_state_class = cv.enum(sensor.STATE_CLASSES, lower=True, space="_")
_validate_filters = cv.validate_registry("filter", sensor.FILTER_REGISTRY)

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
    CONF_FILTERS: [{CONF_MULTIPLY: 0.01}],  # Device sends voltage * 100
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
    CONF_FILTERS: [{CONF_MULTIPLY: 0.01}],  # Device sends temperature * 100
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

# Define sensor type configurations by prefix pattern
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
        "P": POWER_CONFIG,  # Power readings
        "D": POWER_CONFIG,  # Diverter power
        "R": POWER_CONFIG,  # Power reading for single R tag
        "V": VOLTAGE_CONFIG,  # Voltage readings
        "E": ENERGY_CONFIG,  # Energy readings
    },
}


# Create a base schema that's flexible for any tag
# Note: Don't set defaults here for values that vary by tag type
# (accuracy_decimals, state_class) - let tag-specific configs set them
BASE_SCHEMA = sensor.sensor_schema(
    Mk2PVRouterSensor,
).extend(MK2PVROUTER_LISTENER_SCHEMA)


def apply_tag_defaults(config):
    """Apply defaults based on tag pattern or exact match"""
    tag = config[CONF_TAG_NAME]
    tag_upper = tag.upper()

    defaults = None

    # First, check for exact match (single letter or special tags)
    if tag_upper in SENSOR_CONFIGS["exact"]:
        defaults = SENSOR_CONFIGS["exact"][tag_upper]
    # Then check for pattern match (letter + number)
    elif len(tag_upper) >= 2:
        prefix = tag_upper[0]
        suffix = tag_upper[1:]
        # Check if it matches pattern: letter followed by digits
        if prefix in SENSOR_CONFIGS["patterns"] and suffix.isdigit():
            defaults = SENSOR_CONFIGS["patterns"][prefix]

    # Apply defaults if found
    if defaults:
        for key, value in defaults.items():
            if key == CONF_FILTERS:
                # Validate and prepend default filters to user's filters
                validated_filters = _validate_filters(value)
                user_filters = config.get(key, [])
                config[key] = validated_filters + user_filters
            elif key == CONF_STATE_CLASS:
                # Validate state_class to convert string to enum
                if key not in config:
                    config[key] = _validate_state_class(value)
            elif key not in config:
                config[key] = value

    # Fallback: ensure defaults for unknown tags
    if CONF_ACCURACY_DECIMALS not in config:
        config[CONF_ACCURACY_DECIMALS] = 0
    if CONF_STATE_CLASS not in config:
        config[CONF_STATE_CLASS] = _validate_state_class(STATE_CLASS_MEASUREMENT)

    return config


CONFIG_SCHEMA = cv.All(BASE_SCHEMA, apply_tag_defaults)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    mk2pvrouter = await cg.get_variable(config[CONF_MK2PVROUTER_ID])
    cg.add(mk2pvrouter.register_mk2pvrouter_listener(var))
    register_listener()
