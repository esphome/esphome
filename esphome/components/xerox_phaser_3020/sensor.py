import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import STATE_CLASS_TOTAL_INCREASING
from esphome.types import ConfigType

from . import CONF_XEROX_PHASER_3020_ID, XeroxPhaser3020

DEPENDENCIES = ["xerox_phaser_3020"]

CONF_PAGE_COUNT = "page_count"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_XEROX_PHASER_3020_ID): cv.use_id(XeroxPhaser3020),
        # Pages the printer has printed in its life, read every status_interval
        cv.Optional(CONF_PAGE_COUNT): sensor.sensor_schema(
            unit_of_measurement="pages",
            icon="mdi:counter",
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_XEROX_PHASER_3020_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_PAGE_COUNT, hub.set_page_count_sensor)
