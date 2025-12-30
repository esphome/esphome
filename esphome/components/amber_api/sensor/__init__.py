import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    DEVICE_CLASS_MONETARY,
    STATE_CLASS_MEASUREMENT,
)

from .. import CONF_AMBER_API_ID, AmberApiComponent, amber_api_ns

DEPENDENCIES = ["amber_api"]

# Unit for Australian dollars per kilowatt-hour
UNIT_DOLLARS_PER_KWH = "$/kWh"
ICON_CURRENCY_USD = "mdi:currency-usd"

AmberApiSensor = amber_api_ns.class_("AmberApiSensor", sensor.Sensor, cg.Component)

AmberApiSensorType = amber_api_ns.enum("AmberApiSensorType", is_class=False)
SENSOR_TYPES = {
    "general": AmberApiSensorType.GENERAL,
    "general_forecast": AmberApiSensorType.GENERAL_FORECAST,
    "feedin": AmberApiSensorType.FEEDIN,
    "feedin_forecast": AmberApiSensorType.FEEDIN_FORECAST,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        AmberApiSensor,
        unit_of_measurement=UNIT_DOLLARS_PER_KWH,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_MONETARY,
        state_class=STATE_CLASS_MEASUREMENT,
        icon=ICON_CURRENCY_USD,
    )
    .extend(
        {
            cv.GenerateID(CONF_AMBER_API_ID): cv.use_id(AmberApiComponent),
            cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_sensor_type(config[CONF_TYPE]))

    parent = await cg.get_variable(config[CONF_AMBER_API_ID])
    cg.add(parent.register_listener(var))
