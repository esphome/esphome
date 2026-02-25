import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from . import BTHomeSensor, Device, add_handler
from .bthome import BTHOME_OBJECT_TYPE_MAPPING

CODEOWNERS = ["@jpeletier"]

DEPENDENCIES = ["esp32_ble_tracker"]

CONF_REMOTE_ID = "remote_id"
CONF_OBJECT_TYPE = "object_type"


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
            cv.Required(CONF_OBJECT_TYPE): cv.enum(BTHOME_OBJECT_TYPE_MAPPING),
        }
    )
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    cg.add(var.set_object_type(config[CONF_OBJECT_TYPE]))
    await add_handler(var, config[CONF_REMOTE_ID])
    await cg.register_component(var, config)
