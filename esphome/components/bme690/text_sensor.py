import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_IAQ_ACCURACY

from . import CONF_BME690_ID, BME690Component

DEPENDENCIES = ["bme690"]

TYPES = [CONF_IAQ_ACCURACY]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BME690_ID): cv.use_id(BME690Component),
        cv.Optional(CONF_IAQ_ACCURACY): text_sensor.text_sensor_schema(),
    }
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await text_sensor.new_text_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BME690_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
