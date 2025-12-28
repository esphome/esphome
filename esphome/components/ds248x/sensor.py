from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_INDEX

from . import (
    CONF_DS248X_ID,
    DS248xComponent,
    DS248xSensor,
    ds248x_sensor_schema,
    register_ds248x_sensor,
)

DEPENDENCIES = ["ds248x"]

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        unit_of_measurement="°C",
        accuracy_decimals=1,
        device_class="temperature",
        state_class="measurement",
    )
    .extend(ds248x_sensor_schema())
    .extend(
        {
            cv.GenerateID(CONF_DS248X_ID): cv.use_id(DS248xComponent),
            cv.GenerateID(): cv.declare_id(DS248xSensor),
        }
    ),
    cv.has_at_least_one_key(CONF_ADDRESS, CONF_INDEX),
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await register_ds248x_sensor(var, config)
