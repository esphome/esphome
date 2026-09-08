import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TARGET, DEVICE_CLASS_OCCUPANCY
from esphome.types import ConfigType

from . import LD6002BComponent
from .const import AREA_COUNT, CONF_LD6002B_ID, MAX_TARGETS

DEPENDENCIES = ["ld6002b"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
            cv.Optional(CONF_TARGET): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_OCCUPANCY,
            ),
        }
    )
    .extend(
        {
            cv.Optional(f"target_{i + 1}"): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_OCCUPANCY,
            )
            for i in range(MAX_TARGETS)
        }
    )
    .extend(
        {
            cv.Optional(f"detection_area_{i}"): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_OCCUPANCY,
            )
            for i in range(AREA_COUNT)
        }
    )
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD6002B_ID])

    if target_config := config.get(CONF_TARGET):
        sens = await binary_sensor.new_binary_sensor(target_config)
        cg.add(hub.set_presence_binary_sensor(sens))

    for i in range(MAX_TARGETS):
        if target_config := config.get(f"target_{i + 1}"):
            sens = await binary_sensor.new_binary_sensor(target_config)
            cg.add(hub.set_target_presence_binary_sensor(i, sens))

    for i in range(AREA_COUNT):
        if area_config := config.get(f"detection_area_{i}"):
            sens = await binary_sensor.new_binary_sensor(area_config)
            cg.add(hub.set_area_presence_binary_sensor(i, sens))
