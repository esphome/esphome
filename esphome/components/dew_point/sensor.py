import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_HUMIDITY, CONF_TEMPERATURE
from esphome.types import ConfigType

DEPENDENCIES = ["sensor"]

dew_point_ns = cg.esphome_ns.namespace("dew_point")
DewPointComponent = dew_point_ns.class_(
    "DewPointComponent", cg.Component, sensor.Sensor
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        DewPointComponent, kind=sensor.TEMPERATURE, icon="mdi:weather-rainy"
    )
    .extend(
        {
            cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    await cg.add_variable(var.set_temperature_sensor, config[CONF_TEMPERATURE])
    await cg.add_variable(var.set_humidity_sensor, config[CONF_HUMIDITY])
