import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_STATUS
from . import DalyBmsComponent, CONF_BMS_DALY_ID

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


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await text_sensor.new_text_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BMS_DALY_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
