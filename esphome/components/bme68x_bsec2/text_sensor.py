import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_IAQ_ACCURACY

from . import CONF_BME68X_BSEC2_ID, BME68xBSEC2Component

DEPENDENCIES = ["bme68x_bsec2"]

ICON_ACCURACY = "mdi:checkbox-marked-circle-outline"

TYPES = [CONF_IAQ_ACCURACY]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BME68X_BSEC2_ID): cv.use_id(BME68xBSEC2Component),
        cv.Optional(CONF_IAQ_ACCURACY): text_sensor.text_sensor_schema(
            icon=ICON_ACCURACY
        ),
    }
)


async def setup_conf(config, key, hub):
    if conf := config.get(key):
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BME68X_BSEC2_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
