import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from . import BME680BSECComponent, CONF_BME680_BSEC_ID

DEPENDENCIES = ["bme680_bsec"]

CONF_IAQ_ACCURACY = "iaq_accuracy"
ICON_ACCURACY = "mdi:checkbox-marked-circle-outline"

TYPES = [CONF_IAQ_ACCURACY]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BME680_BSEC_ID): cv.use_id(BME680BSECComponent),
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
    hub = await cg.get_variable(config[CONF_BME680_BSEC_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
