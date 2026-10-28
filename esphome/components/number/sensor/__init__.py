import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE_ID
from esphome.types import ConfigType

from .. import Number

CONFIG_SCHEMA = sensor.sensor_schema(sensor.Sensor).extend(
    {
        cv.Required(CONF_SOURCE_ID): cv.use_id(Number),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    # Number has the same float state callback as Sensor
    sensor.follow_sensor(var, await cg.get_variable(config[CONF_SOURCE_ID]))
