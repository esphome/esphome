import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_STATUS
from esphome.types import ConfigType

from . import CONF_XEROX_PHASER_3020_ID, XeroxPhaser3020

DEPENDENCIES = ["xerox_phaser_3020"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_XEROX_PHASER_3020_ID): cv.use_id(XeroxPhaser3020),
        # The printer's PJL status in words, read every status_interval
        cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:printer",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_XEROX_PHASER_3020_ID])
    sensors = text_sensor.sub_text_sensors(config)
    await sensors(CONF_STATUS, hub.set_status_text_sensor)
