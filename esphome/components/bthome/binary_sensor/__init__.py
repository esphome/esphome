import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_DEVICE_CLASS, CONF_TYPE

from .. import BTHomeBinarySensor, RemoteDevice, add_handler
from ..bthome import (
    BTHOME_OBJECT_TYPES,
    BTHomeObjectTypeKind,
    bthome_object_type_validator,
    bthome_object_types,
)

CODEOWNERS = ["@jpeletier"]

DEPENDENCIES = ["bthome", "binary_sensor"]

CONF_REMOTE_ID = "remote_id"


def _apply_object_type_defaults(config):
    """Fill in binary sensor schema defaults based on the chosen object_type, if not already set."""
    obj = BTHOME_OBJECT_TYPES[config[CONF_TYPE]]
    config = dict(config)
    if CONF_DEVICE_CLASS not in config and obj.device_class is not None:
        config[CONF_DEVICE_CLASS] = obj.device_class
    return config


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(class_=BTHomeBinarySensor).extend(
        {
            cv.Required(CONF_REMOTE_ID): cv.use_id(RemoteDevice),
            cv.Required(CONF_TYPE): bthome_object_type_validator(
                BTHomeObjectTypeKind.BINARY_SENSOR
            ),
        }
    ),
    _apply_object_type_defaults,
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    object_type_key = config[CONF_TYPE]
    cg.add(var.set_object_type(bthome_object_types.__getattr__(object_type_key)))
    await add_handler(
        var,
        config[CONF_REMOTE_ID],
        BTHOME_OBJECT_TYPES[object_type_key].object_id,
    )
    await cg.register_component(var, config)
