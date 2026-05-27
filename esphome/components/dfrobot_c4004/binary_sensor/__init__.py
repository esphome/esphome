import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import CONF_C4004_ID, C4004Component

CONF_ONLINE = "online"
CONF_PRESENCE = "presence"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4004_ID): cv.use_id(C4004Component),
        cv.Optional(CONF_ONLINE): binary_sensor.binary_sensor_schema(
            device_class="connectivity",
            icon="mdi:radar",
        ),
        cv.Optional(CONF_PRESENCE): binary_sensor.binary_sensor_schema(
            device_class="occupancy",
            icon="mdi:human-handsup",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_C4004_ID])

    if online_config := config.get(CONF_ONLINE):
        sens = await binary_sensor.new_binary_sensor(online_config)
        cg.add(parent.set_online_binary_sensor(sens))

    if presence_config := config.get(CONF_PRESENCE):
        sens = await binary_sensor.new_binary_sensor(presence_config)
        cg.add(parent.set_presence_binary_sensor(sens))
