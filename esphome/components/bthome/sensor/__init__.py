import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
)

from .. import BTHomeSensor, RemoteDevice, add_handler
from ..bthome import (
    BTHOME_OBJECT_TYPES,
    CONF_BTHOME_TYPE,
    BTHomeObjectTypeKind,
    bthome_object_type_validator,
    bthome_object_types,
)

CODEOWNERS = ["@jpeletier"]

DEPENDENCIES = ["bthome", "sensor"]

CONF_REMOTE_ID = "remote_id"


def _apply_object_type_defaults(config):
    """Fill in sensor schema defaults based on the chosen object_type, if not already set."""
    obj = BTHOME_OBJECT_TYPES[config[CONF_BTHOME_TYPE]]
    config = dict(config)
    if CONF_UNIT_OF_MEASUREMENT not in config:
        config[CONF_UNIT_OF_MEASUREMENT] = obj.unit
    if CONF_ACCURACY_DECIMALS not in config:
        config[CONF_ACCURACY_DECIMALS] = 2
    if CONF_DEVICE_CLASS not in config and obj.device_class is not None:
        config[CONF_DEVICE_CLASS] = obj.device_class
    if CONF_STATE_CLASS not in config:
        config[CONF_STATE_CLASS] = sensor.validate_state_class(obj.state_class)
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(class_=BTHomeSensor).extend(
        {
            cv.Required(CONF_REMOTE_ID): cv.use_id(RemoteDevice),
            cv.Required(CONF_BTHOME_TYPE): bthome_object_type_validator(
                BTHomeObjectTypeKind.SENSOR
            ),
        }
    ),
    _apply_object_type_defaults,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    object_type_key = config[CONF_BTHOME_TYPE]
    cg.add(var.set_object_type(bthome_object_types.__getattr__(object_type_key)))
    await add_handler(
        var,
        config[CONF_REMOTE_ID],
        BTHOME_OBJECT_TYPES[object_type_key].object_id,
    )
    await cg.register_component(var, config)
