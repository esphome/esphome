import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TARGET, DEVICE_CLASS_OCCUPANCY

from . import LD6002BComponent
from .const import CONF_LD6002B_ID

DEPENDENCIES = ["ld6002b"]

MAX_TARGETS = 3

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_TARGET): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_OCCUPANCY,
        ),
    }
).extend(
    {
        cv.Optional(f"target_{i + 1}"): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_OCCUPANCY,
        )
        for i in range(MAX_TARGETS)
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LD6002B_ID])

    if target_config := config.get(CONF_TARGET):
        sens = await binary_sensor.new_binary_sensor(target_config)
        cg.add(hub.set_presence_binary_sensor(sens))

    for i in range(MAX_TARGETS):
        if target_config := config.get(f"target_{i + 1}"):
            sens = await binary_sensor.new_binary_sensor(target_config)
            cg.add(hub.set_target_presence_binary_sensor(i, sens))
