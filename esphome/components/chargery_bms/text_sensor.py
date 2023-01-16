import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from . import ChargeryBmsComponent, CONF_CHARGERY_BMS_ID

ICON_CAR_BATTERY = "mdi:car-battery"

CONF_CURRENT_MODE = "current_mode"
CONF_CURRENT1_MODE = "current1_mode"

TYPES = [CONF_CURRENT_MODE, CONF_CURRENT1_MODE]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_CHARGERY_BMS_ID): cv.use_id(ChargeryBmsComponent),
            cv.Optional(CONF_CURRENT_MODE): text_sensor.text_sensor_schema(
                icon=ICON_CAR_BATTERY
            ),
            cv.Optional(CONF_CURRENT1_MODE): text_sensor.text_sensor_schema(
                icon=ICON_CAR_BATTERY
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if key in config:
        conf = config[key]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_CHARGERY_BMS_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
