import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import BTHomeTextSensor, RemoteDevice, add_handler
from ..bthome import (
    BTHOME_OBJECT_TYPES,
    CONF_BTHOME_TYPE,
    BTHomeObjectTypeKind,
    bthome_object_type_validator,
    bthome_object_types,
)

CODEOWNERS = ["@jpeletier"]

DEPENDENCIES = ["bthome", "text_sensor"]

CONF_REMOTE_ID = "remote_id"

CONFIG_SCHEMA = text_sensor.text_sensor_schema(class_=BTHomeTextSensor).extend(
    {
        cv.Required(CONF_REMOTE_ID): cv.use_id(RemoteDevice),
        cv.Required(CONF_BTHOME_TYPE): bthome_object_type_validator(
            BTHomeObjectTypeKind.TEXT_SENSOR
        ),
    }
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    object_type_key = config[CONF_BTHOME_TYPE]
    cg.add(var.set_object_type(bthome_object_types.__getattr__(object_type_key)))
    await add_handler(
        var,
        config[CONF_REMOTE_ID],
        BTHOME_OBJECT_TYPES[object_type_key].object_id,
    )
    await cg.register_component(var, config)
