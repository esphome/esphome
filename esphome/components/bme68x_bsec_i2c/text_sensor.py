import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_IAQ_ACCURACY

from . import BME68xBSECI2CComponent, CONF_BME68X_BSEC_I2C_ID

DEPENDENCIES = ["bme68x_bsec_i2c"]

ICON_ACCURACY = "mdi:checkbox-marked-circle-outline"

TYPES = [CONF_IAQ_ACCURACY]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BME68X_BSEC_I2C_ID): cv.use_id(BME68xBSECI2CComponent),
        cv.Optional(CONF_IAQ_ACCURACY): text_sensor.text_sensor_schema(
            icon=ICON_ACCURACY
        ),
    }
)


async def setup_conf(config, key, hub):
    if key in config:
        conf = config[key]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BME68X_BSEC_I2C_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
