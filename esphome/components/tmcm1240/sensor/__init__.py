import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import TMCM1240, tmcm1240_ns

DEPENDENCIES = ["tmcm1240"]
CONF_TMCM1240_ID = "tmcm1240_id"
TMCM1240Sensor = tmcm1240_ns.class_(
    "TMCM1240Sensor", sensor.Sensor, cg.PollingComponent
)
TYPES = {
    "STALLGUARD_LOAD": (0x2101, 2),
    "DRIVER_ERROR_FLAGS": (0x2102, 1),
    "STALLGUARD_THRESHOLD": (0x209E, 1),
    "STALLGUARD_MIN_VELOCITY": (0x20A4, 4),
}
CONFIG_SCHEMA = (
    sensor.sensor_schema(TMCM1240Sensor, accuracy_decimals=0)
    .extend(
        {
            cv.GenerateID(CONF_TMCM1240_ID): cv.use_id(TMCM1240),
            cv.Required(CONF_TYPE): cv.one_of(*TYPES, upper=True),
        }
    )
    .extend(cv.polling_component_schema("1s"))
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_TMCM1240_ID])
    index, size = TYPES[config[CONF_TYPE]]
    var = await sensor.new_sensor(config, parent, index, size)
    await cg.register_component(var, config)
