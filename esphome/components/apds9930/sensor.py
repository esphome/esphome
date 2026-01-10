import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)

from . import APDS9930, CONF_APDS9930_ID

DEPENDENCIES = ["apds9930"]

TYPES = ["ambient_light", "proximity"]

CONFIG_SCHEMA = cv.typed_schema(
    {
        "ambient_light": sensor.sensor_schema(
            unit_of_measurement=UNIT_LUX,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        "proximity": sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    },
    lower=True,
).extend(
    {
        cv.GenerateID(CONF_APDS9930_ID): cv.use_id(APDS9930),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_APDS9930_ID])
    var = await sensor.new_sensor(config)
    sensor_type = config[CONF_TYPE]
    func = getattr(hub, f"set_{sensor_type}_sensor")
    cg.add(func(var))
