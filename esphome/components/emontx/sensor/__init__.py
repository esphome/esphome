import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ID,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_PULSES,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)
from esphome.types import ConfigType

from .. import CONF_EMONTX_ID, CONF_TAG_NAME, EmonTx, emontx_ns

EmonTxSensor = emontx_ns.class_("EmonTxSensor", sensor.Sensor, cg.Component)

# Define sensor type configurations by prefix
SENSOR_CONFIGS = {
    "P": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_WATT,
        CONF_DEVICE_CLASS: DEVICE_CLASS_POWER,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 0,
    },
    "E": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_WATT_HOURS,
        CONF_DEVICE_CLASS: DEVICE_CLASS_ENERGY,
        CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
        CONF_ACCURACY_DECIMALS: 0,
    },
    "V": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_VOLT,
        CONF_DEVICE_CLASS: DEVICE_CLASS_VOLTAGE,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 2,
    },
    "I": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_AMPERE,
        CONF_DEVICE_CLASS: DEVICE_CLASS_CURRENT,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 2,
    },
    "T": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
        CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 2,
    },
}

# Pattern-based configurations
PATTERN_CONFIGS = {
    "PULSE": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_PULSES,
        CONF_DEVICE_CLASS: DEVICE_CLASS_ENERGY,
        CONF_ACCURACY_DECIMALS: 0,
    },
    "PF": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_EMPTY,
        CONF_DEVICE_CLASS: DEVICE_CLASS_POWER_FACTOR,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 2,
    },
}

# Create a base schema that's flexible for any tag
BASE_SCHEMA = sensor.sensor_schema(
    EmonTxSensor,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=0,
).extend(
    {
        cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)


def apply_tag_defaults(config: ConfigType) -> ConfigType:
    """Apply defaults based on tag prefix if applicable, but don't restrict any tags."""
    tag = config[CONF_TAG_NAME]

    # Skip if tag is too short
    if len(tag) < 2:
        return config

    # Check if this tag starts with a known prefix
    tag_upper = tag.upper()

    for pattern, pattern_config in PATTERN_CONFIGS.items():
        if tag_upper.startswith(pattern):
            # Apply pattern defaults if not overridden by user
            for key, value in pattern_config.items():
                if key not in config:
                    config[key] = value
            return config

    # Only apply defaults for known prefixes with numeric indices
    prefix = tag_upper[0]
    if prefix in SENSOR_CONFIGS and len(tag) > 1 and tag[1:].isdigit():
        # Apply defaults for known tag types, but only if not overridden by user
        defaults = SENSOR_CONFIGS[prefix]
        for key, value in defaults.items():
            if key not in config:
                config[key] = value

    return config


CONFIG_SCHEMA = cv.All(BASE_SCHEMA, apply_tag_defaults)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    hub = await cg.get_variable(config[CONF_EMONTX_ID])
    cg.add(hub.register_sensor(config[CONF_TAG_NAME], var))
