from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import time as time_
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import (
    CONF_GEOLOCATION_ID,
    FIX,
    FIX_PUBLISHER,
    GEOLOCATION_CHILD_SCHEMA,
    geolocation_ns,
)

DEPENDENCIES = ["geolocation"]

GeolocationTime = geolocation_ns.class_("GeolocationTime", time_.RealTimeClock)

CONFIG_SCHEMA = (
    time_.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GeolocationTime),
        }
    )
    .extend(GEOLOCATION_CHILD_SCHEMA)
    .extend(cv.polling_component_schema("5min"))
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_GEOLOCATION_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await time_.register_time(var, config)
    await cg.register_component(var, config)
    FIX_PUBLISHER.add(parent, parent_ref(var).on_fix(FIX))
