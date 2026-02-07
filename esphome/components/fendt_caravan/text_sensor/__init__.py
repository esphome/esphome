import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, ENTITY_CATEGORY_CONFIG

from .. import CONF_PARENT_ID, CaravanDeviceComponent, fendt_caravan_ns

FendtTextSensor = fendt_caravan_ns.class_(
    "FendtTextSensor",
    text_sensor.TextSensor,
    cg.Component,
    cg.Parented.template(CaravanDeviceComponent),
)

FENDT_TEXT_SENSOR_SCHEMA = text_sensor.text_sensor_schema(
    FendtTextSensor, entity_category=ENTITY_CATEGORY_CONFIG
).extend(
    {
        cv.Required(CONF_PARENT_ID): cv.use_id(CaravanDeviceComponent),
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        "power_status": FENDT_TEXT_SENSOR_SCHEMA,
        "software_version": FENDT_TEXT_SENSOR_SCHEMA,
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_text_sensor")(var))
