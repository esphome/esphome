import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_HIDE_TIMESTAMP, ENTITY_CATEGORY_DIAGNOSTIC, ICON_NEW_BOX

version_ns = cg.esphome_ns.namespace("version")
VersionTextSensor = version_ns.class_(
    "VersionTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(
        icon=ICON_NEW_BOX,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(VersionTextSensor),
            cv.Optional(CONF_HIDE_TIMESTAMP, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_hide_timestamp(config[CONF_HIDE_TIMESTAMP]))
