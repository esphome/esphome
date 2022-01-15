from esphome.components import time as time_
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID
from .. import gps_ns, GPSListener, CONF_GPS_ID, GPS

DEPENDENCIES = ["gps"]

GPSTime = gps_ns.class_(
    "GPSTime", cg.PollingComponent, time_.RealTimeClock, GPSListener
)

CONFIG_SCHEMA = time_.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GPSTime),
        cv.GenerateID(CONF_GPS_ID): cv.use_id(GPS),
    }
).extend(cv.polling_component_schema("5min"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await time_.register_time(var, config)
    await cg.register_component(var, config)

    paren = await cg.get_variable(config[CONF_GPS_ID])
    cg.add(paren.register_listener(var))
