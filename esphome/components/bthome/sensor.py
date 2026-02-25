from esphome import core
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from esphome.cpp_generator import TemplateArguments

from . import BTHomeSensor, BTHomeSensorBase, Device, add_sensor
from .bthome import BTHOME_OBJECT_TYPE_MAPPING

CODEOWNERS = ["@jpeletier"]

DEPENDENCIES = ["esp32_ble_tracker"]

CONF_REMOTE_ID = "remote_id"
CONF_OBJECT_TYPE = "object_type"


OBJECT_TYPE_SCHEMA = cv.enum(BTHOME_OBJECT_TYPE_MAPPING)


CONFIG_SCHEMA = sensor.sensor_schema(
    class_=BTHomeSensor,
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    cv.Schema(
        {
            cv.Required(CONF_REMOTE_ID): cv.use_id(Device),
            cv.Required(CONF_OBJECT_TYPE): cv.ensure_list(OBJECT_TYPE_SCHEMA),
        }
    )
)


async def to_code(config):
    sensor_id = config[CONF_ID]
    base_id = core.ID(str(sensor_id), False, BTHomeSensorBase)
    object_types = config[CONF_OBJECT_TYPE]

    var = cg.Pvariable(
        base_id, sensor_id.type.template(TemplateArguments(len(object_types))).new()
    )
    cg.add(var.set_object_types(object_types))
    await sensor.register_sensor(var, config)
    await add_sensor(var, config[CONF_REMOTE_ID])
    await cg.register_component(var, config)
