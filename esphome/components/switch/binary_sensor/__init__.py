import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE_ID
from esphome.core.entity_helpers import FollowEntity
from esphome.types import ConfigType

from .. import Switch

CODEOWNERS = ["@ssieb"]

_FOLLOW_SWITCH = FollowEntity("add_on_state_callback", ((cg.bool_, "x"),), "x")

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(binary_sensor.BinarySensor).extend(
    {
        cv.Required(CONF_SOURCE_ID): cv.use_id(Switch),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    _FOLLOW_SWITCH(var, await cg.get_variable(config[CONF_SOURCE_ID]))
