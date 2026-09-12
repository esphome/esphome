import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC

from . import (
    ECOCOMFORT2_CLIENT_SCHEMA,
    Ecocomfort2TextSensor,
    register_ecocomfort2_child,
)
from .const import CONF_FIRMWARE

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Ecocomfort2TextSensor),
            cv.Optional(CONF_FIRMWARE): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECOCOMFORT2_CLIENT_SCHEMA)
    .add_extra(cv.has_at_least_one_key(CONF_FIRMWARE))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_ecocomfort2_child(var, config)

    if conf := config.get(CONF_FIRMWARE):
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(var.set_firmware_sensor(sens))
