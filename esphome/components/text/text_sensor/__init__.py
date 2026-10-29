import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE_ID
from esphome.types import ConfigType

from .. import Text

CONFIG_SCHEMA = text_sensor.text_sensor_schema(text_sensor.TextSensor).extend(
    {
        cv.Required(CONF_SOURCE_ID): cv.use_id(Text),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    # Text has the same string state callback as TextSensor
    text_sensor.follow_text_sensor(var, await cg.get_variable(config[CONF_SOURCE_ID]))
