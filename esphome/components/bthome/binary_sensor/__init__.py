import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_DEVICE_CLASS, CONF_SOURCE_ID, CONF_TYPE
from esphome.types import ConfigType

from .. import BTHomeRemoteObject, RemoteDevice, add_handler, client_ns
from ..bthome import bthome_object_type_validator, bthome_object_types
from .object_types import BINARY_SENSOR_OBJECT_TYPES

CODEOWNERS = ["@jpeletier"]

DEPENDENCIES = ["bthome", "binary_sensor"]

BTHomeBinarySensor = client_ns.class_(
    "BTHomeBinarySensor", BTHomeRemoteObject, binary_sensor.BinarySensor
)


def _apply_object_type_defaults(config: ConfigType) -> ConfigType:
    """Fill in binary sensor schema defaults based on the chosen object_type, if not already set."""
    obj = BINARY_SENSOR_OBJECT_TYPES[config[CONF_TYPE]]
    config = dict(config)
    if CONF_DEVICE_CLASS not in config and obj.device_class is not None:
        config[CONF_DEVICE_CLASS] = obj.device_class
    return config


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(class_=BTHomeBinarySensor).extend(
        {
            cv.Required(CONF_SOURCE_ID): cv.use_id(RemoteDevice),
            cv.Required(CONF_TYPE): bthome_object_type_validator(
                BINARY_SENSOR_OBJECT_TYPES
            ),
        }
    ),
    _apply_object_type_defaults,
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    object_type_key = config[CONF_TYPE]
    cg.add(var.set_object_type(getattr(bthome_object_types, object_type_key)))
    add_handler(
        var,
        config[CONF_SOURCE_ID],
        BINARY_SENSOR_OBJECT_TYPES[object_type_key].object_id,
    )
