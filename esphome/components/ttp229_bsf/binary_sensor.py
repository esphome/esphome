from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL
from esphome.types import ConfigType

from . import (
    CHANNELS_PUBLISHER,
    CONF_TTP229_ID,
    TOUCHED,
    TTP229BSFComponent,
    ttp229_bsf_ns,
)

DEPENDENCIES = ["ttp229_bsf"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_TTP229_ID): cv.use_id(TTP229BSFComponent),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_TTP229_ID])
    CHANNELS_PUBLISHER.add(
        hub,
        ttp229_bsf_ns.publish_channel(parent_ref(var), TOUCHED, config[CONF_CHANNEL]),
    )
