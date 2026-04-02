import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.const import CONF_CLIMATE_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC

from ..climate import EquithermClimate, equitherm_ns

# Explicit binary sensor classes for each type
OutdoorSensorFaultBinarySensor = equitherm_ns.class_(
    "OutdoorSensorFaultBinarySensor", binary_sensor.BinarySensor, cg.Component
)
IndoorSensorFaultBinarySensor = equitherm_ns.class_(
    "IndoorSensorFaultBinarySensor", binary_sensor.BinarySensor, cg.Component
)
RateLimitingBinarySensor = equitherm_ns.class_(
    "RateLimitingBinarySensor", binary_sensor.BinarySensor, cg.Component
)
PidActiveBinarySensor = equitherm_ns.class_(
    "PidActiveBinarySensor", binary_sensor.BinarySensor, cg.Component
)

# Configuration keys for each binary sensor type
CONF_OUTDOOR_SENSOR_FAULT = "outdoor_sensor_fault"
CONF_INDOOR_SENSOR_FAULT = "indoor_sensor_fault"
CONF_RATE_LIMITING_ACTIVE = "rate_limiting_active"
CONF_PID_ACTIVE = "pid_active"


def _problem_sensor_schema(binary_sensor_class, icon="mdi:alert-circle-outline"):
    """Generate schema for problem-indicating binary sensors (fallback sensors)."""
    return binary_sensor.binary_sensor_schema(
        binary_sensor_class,
        device_class=DEVICE_CLASS_PROBLEM,
        icon=icon,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.COMPONENT_SCHEMA)


def _status_sensor_schema(binary_sensor_class, icon="mdi:alert-circle-outline"):
    """Generate schema for status binary sensors (normal operation indicators)."""
    return binary_sensor.binary_sensor_schema(
        binary_sensor_class,
        icon=icon,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(EquithermClimate),
        cv.Optional(CONF_OUTDOOR_SENSOR_FAULT): _problem_sensor_schema(
            OutdoorSensorFaultBinarySensor
        ),
        cv.Optional(CONF_INDOOR_SENSOR_FAULT): _problem_sensor_schema(
            IndoorSensorFaultBinarySensor
        ),
        cv.Optional(CONF_RATE_LIMITING_ACTIVE): _status_sensor_schema(
            RateLimitingBinarySensor, icon="mdi:speedometer-slow"
        ),
        cv.Optional(CONF_PID_ACTIVE): _status_sensor_schema(
            PidActiveBinarySensor, icon="mdi:tune-vertical"
        ),
    }
)


async def _register_binary_sensor(config, parent_id):
    """Helper to register a binary sensor entity."""
    bs = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(bs, config)
    await cg.register_parented(bs, parent_id)
    return bs


async def to_code(config):
    parent_id = config[CONF_CLIMATE_ID]

    if outdoor_config := config.get(CONF_OUTDOOR_SENSOR_FAULT):
        await _register_binary_sensor(outdoor_config, parent_id)

    if indoor_config := config.get(CONF_INDOOR_SENSOR_FAULT):
        await _register_binary_sensor(indoor_config, parent_id)

    if rate_limiting_config := config.get(CONF_RATE_LIMITING_ACTIVE):
        await _register_binary_sensor(rate_limiting_config, parent_id)

    if pid_active_config := config.get(CONF_PID_ACTIVE):
        await _register_binary_sensor(pid_active_config, parent_id)
