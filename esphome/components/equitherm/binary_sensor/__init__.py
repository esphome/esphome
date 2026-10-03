import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.const import CONF_CLIMATE_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC

from ..climate import EquithermClimate, equitherm_ns

# Explicit binary sensor classes for each type
RateLimitingBinarySensor = equitherm_ns.class_(
    "RateLimitingBinarySensor", binary_sensor.BinarySensor, cg.Component
)

# Configuration keys for each binary sensor type
CONF_RATE_LIMITING_ACTIVE = "rate_limiting_active"


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
        cv.Optional(CONF_RATE_LIMITING_ACTIVE): _status_sensor_schema(
            RateLimitingBinarySensor, icon="mdi:speedometer-slow"
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

    if rate_limiting_config := config.get(CONF_RATE_LIMITING_ACTIVE):
        await _register_binary_sensor(rate_limiting_config, parent_id)
