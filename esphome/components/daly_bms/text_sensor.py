import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_STATUS
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import CONF_BMS_DALY_ID, DalyBmsComponent

ICON_CAR_BATTERY = "mdi:car-battery"

TYPES = [
    CONF_STATUS,
]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_BMS_DALY_ID): cv.use_id(DalyBmsComponent),
            cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(
                icon=ICON_CAR_BATTERY
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config: ConfigType, key: str, hub: MockObj) -> None:
    if sensor_config := config.get(key):
        sens = await text_sensor.new_text_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BMS_DALY_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
