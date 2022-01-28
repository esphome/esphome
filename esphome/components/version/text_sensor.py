import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ID,
    CONF_ICON,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_NEW_BOX,
    CONF_HIDE_TIMESTAMP,
)

version_ns = cg.esphome_ns.namespace("version")
VersionTextSensor = version_ns.class_(
    "VersionTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(VersionTextSensor),
        cv.Optional(CONF_ICON, default=ICON_NEW_BOX): text_sensor.icon,
        cv.Optional(CONF_HIDE_TIMESTAMP, default=False): cv.boolean,
        cv.Optional(
            CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_DIAGNOSTIC
        ): cv.entity_category,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await text_sensor.register_text_sensor(var, config)
    await cg.register_component(var, config)
    cg.add(var.set_hide_timestamp(config[CONF_HIDE_TIMESTAMP]))
