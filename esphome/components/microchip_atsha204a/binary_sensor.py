import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PRESENCE
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import ATSHA204A, CONF_ATSHA204A_ID, DOMAIN

DEPENDENCIES = [DOMAIN]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_PRESENCE
).extend({cv.GenerateID(CONF_ATSHA204A_ID): cv.use_id(ATSHA204A)})


FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=binary_sensor.DOMAIN,
    platform=DOMAIN,
    keys=(CONF_ATSHA204A_ID,),
    message="Only one ATSHA204A binary sensor platform entry is allowed per hub",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ATSHA204A_ID])
    sensor = await binary_sensor.new_binary_sensor(config)
    cg.add(hub.set_presence_binary_sensor(sensor))
