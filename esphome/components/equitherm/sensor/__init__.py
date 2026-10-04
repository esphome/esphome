import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_CLIMATE_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ICON,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from ..climate import EquithermClimate, equitherm_ns

# =============================================================================
# C++ Class Declarations
# =============================================================================

EquithermSensor = equitherm_ns.class_("EquithermSensor", sensor.Sensor, cg.Component)
EquithermSensorType = equitherm_ns.enum("EquithermSensorType")

# =============================================================================
# Sensor Type Enum Mapping
# =============================================================================

EQUITHERM_SENSOR_TYPES = {
    "HEATING_CURVE_OUTPUT": EquithermSensorType.EQUITHERM_SENSOR_TYPE_HEATING_CURVE_OUTPUT,
    "PID_ADJUSTED_OUTPUT": EquithermSensorType.EQUITHERM_SENSOR_TYPE_PID_ADJUSTED_OUTPUT,
    "FLOW_SETPOINT": EquithermSensorType.EQUITHERM_SENSOR_TYPE_FLOW_SETPOINT,
    "ACTIVE_SETPOINT": EquithermSensorType.EQUITHERM_SENSOR_TYPE_ACTIVE_SETPOINT,
    "PID_CORRECTION": EquithermSensorType.EQUITHERM_SENSOR_TYPE_PID_CORRECTION,
    "PID_PROPORTIONAL": EquithermSensorType.EQUITHERM_SENSOR_TYPE_PID_PROPORTIONAL,
    "PID_INTEGRAL": EquithermSensorType.EQUITHERM_SENSOR_TYPE_PID_INTEGRAL,
    "PID_DERIVATIVE": EquithermSensorType.EQUITHERM_SENSOR_TYPE_PID_DERIVATIVE,
    "MIN_FLOW_TEMP": EquithermSensorType.EQUITHERM_SENSOR_TYPE_MIN_FLOW_TEMP,
    "MAX_FLOW_TEMP": EquithermSensorType.EQUITHERM_SENSOR_TYPE_MAX_FLOW_TEMP,
}

# =============================================================================
# Sensor Type Configurations
# =============================================================================


def _temperature_sensor_config():
    """Default configuration for temperature sensors."""
    return {
        "unit": UNIT_CELSIUS,
        "icon": ICON_THERMOMETER,
        "accuracy_decimals": 1,
        "device_class": DEVICE_CLASS_TEMPERATURE,
    }


# Sensor type configurations grouped by category
FLOW_TEMPERATURE_SENSORS = {
    "HEATING_CURVE_OUTPUT": _temperature_sensor_config(),
    "PID_ADJUSTED_OUTPUT": _temperature_sensor_config(),
    "FLOW_SETPOINT": _temperature_sensor_config(),
    "ACTIVE_SETPOINT": _temperature_sensor_config(),
}

PID_DIAGNOSTIC_SENSORS = {
    "PID_CORRECTION": _temperature_sensor_config(),
    "PID_PROPORTIONAL": _temperature_sensor_config(),
    "PID_INTEGRAL": _temperature_sensor_config(),
    "PID_DERIVATIVE": _temperature_sensor_config(),
}

# Diagnostic sensors for output parameters (static, set at compile time)
OUTPUT_PARAMETER_SENSORS = {
    "MIN_FLOW_TEMP": {
        **_temperature_sensor_config(),
        "entity_category": ENTITY_CATEGORY_DIAGNOSTIC,
    },
    "MAX_FLOW_TEMP": {
        **_temperature_sensor_config(),
        "entity_category": ENTITY_CATEGORY_DIAGNOSTIC,
    },
}

# Combined sensor type configurations
SENSOR_TYPE_CONFIGS = {
    **FLOW_TEMPERATURE_SENSORS,
    **PID_DIAGNOSTIC_SENSORS,
    **OUTPUT_PARAMETER_SENSORS,
}

# =============================================================================
# Validation
# =============================================================================


def _apply_type_defaults(config):
    """Apply type-specific defaults based on sensor type."""
    type_config = SENSOR_TYPE_CONFIGS[config[CONF_TYPE]]

    config.setdefault(CONF_UNIT_OF_MEASUREMENT, type_config["unit"])
    config.setdefault(CONF_ICON, type_config["icon"])
    config.setdefault(CONF_ACCURACY_DECIMALS, type_config["accuracy_decimals"])

    if type_config["device_class"]:
        config.setdefault(CONF_DEVICE_CLASS, type_config["device_class"])

    if "entity_category" in type_config:
        config["entity_category"] = type_config["entity_category"]

    return config


# =============================================================================
# Configuration Schema
# =============================================================================

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        EquithermSensor,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_NONE,
    )
    .extend(
        {
            cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(EquithermClimate),
            cv.Required(CONF_TYPE): cv.enum(EQUITHERM_SENSOR_TYPES, upper=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _apply_type_defaults,
)

# =============================================================================
# Code Generation
# =============================================================================


async def to_code(config):
    parent = await cg.get_variable(config[CONF_CLIMATE_ID])
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
