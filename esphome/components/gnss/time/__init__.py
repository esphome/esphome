from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import time as time_
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import CONF_GNSS_ID, FIX, FIX_PUBLISHER, GNSS_CHILD_SCHEMA, gnss_ns

DEPENDENCIES = ["gnss"]

GNSSTime = gnss_ns.class_("GNSSTime", time_.RealTimeClock)

CONFIG_SCHEMA = (
    time_.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GNSSTime),
        }
    )
    .extend(GNSS_CHILD_SCHEMA)
    .extend(cv.polling_component_schema("5min"))
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_GNSS_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await time_.register_time(var, config)
    await cg.register_component(var, config)
    FIX_PUBLISHER.add(parent, parent_ref(var).on_fix(FIX))
