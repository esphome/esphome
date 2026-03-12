import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC

from ..climate import EquithermClimate, equitherm_ns

ControlModeTextSensor = equitherm_ns.class_(
    "ControlModeTextSensor", text_sensor.TextSensor, cg.Component
)

CONF_CLIMATE_ID = "climate_id"
CONF_CONTROL_MODE = "control_mode"


def _text_sensor_schema(text_sensor_class, icon="mdi:thermostat"):
    """Generate schema for a specific text sensor type."""
    return text_sensor.text_sensor_schema(
        text_sensor_class,
        icon=icon,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(EquithermClimate),
        cv.Optional(CONF_CONTROL_MODE): _text_sensor_schema(ControlModeTextSensor),
    }
)


async def _register_text_sensor(config, parent_id):
    """Helper to register a text sensor entity."""
    ts = await text_sensor.new_text_sensor(config)
    await cg.register_component(ts, config)
    await cg.register_parented(ts, parent_id)
    return ts


async def to_code(config):
    parent_id = config[CONF_CLIMATE_ID]

    if control_mode_config := config.get(CONF_CONTROL_MODE):
        await _register_text_sensor(control_mode_config, parent_id)
