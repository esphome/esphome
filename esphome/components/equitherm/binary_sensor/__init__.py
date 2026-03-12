import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC

from ..climate import EquithermClimate, equitherm_ns

# Explicit binary sensor classes for each type
OutdoorFallbackBinarySensor = equitherm_ns.class_(
    "OutdoorFallbackBinarySensor", binary_sensor.BinarySensor, cg.Component
)
IndoorFallbackBinarySensor = equitherm_ns.class_(
    "IndoorFallbackBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONF_CLIMATE_ID = "climate_id"

# Configuration keys for each binary sensor type
CONF_OUTDOOR_FALLBACK_ACTIVE = "outdoor_fallback_active"
CONF_INDOOR_FALLBACK_ACTIVE = "indoor_fallback_active"


def _binary_sensor_schema(binary_sensor_class, icon="mdi:alert-circle-outline"):
    """Generate schema for a specific binary sensor type."""
    return binary_sensor.binary_sensor_schema(
        binary_sensor_class,
        device_class=DEVICE_CLASS_PROBLEM,
        icon=icon,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(EquithermClimate),
        cv.Optional(CONF_OUTDOOR_FALLBACK_ACTIVE): _binary_sensor_schema(
            OutdoorFallbackBinarySensor
        ),
        cv.Optional(CONF_INDOOR_FALLBACK_ACTIVE): _binary_sensor_schema(
            IndoorFallbackBinarySensor
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

    if outdoor_config := config.get(CONF_OUTDOOR_FALLBACK_ACTIVE):
        await _register_binary_sensor(outdoor_config, parent_id)

    if indoor_config := config.get(CONF_INDOOR_FALLBACK_ACTIVE):
        await _register_binary_sensor(indoor_config, parent_id)
