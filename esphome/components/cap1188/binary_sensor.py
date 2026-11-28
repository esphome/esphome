from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL
from esphome.types import ConfigType

from . import CHANNELS_PUBLISHER, CONF_CAP1188_ID, TOUCHED, CAP1188Component, cap1188_ns

DEPENDENCIES = ["cap1188"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_CAP1188_ID): cv.use_id(CAP1188Component),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_CAP1188_ID])
    CHANNELS_PUBLISHER.add(
        hub, cap1188_ns.publish_channel(parent_ref(var), TOUCHED, config[CONF_CHANNEL])
    )
