import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_ICON
from esphome.core import coroutine
from . import BME680BSECComponent, CONF_BME680_BSEC_ID

DEPENDENCIES = ["bme680_bsec"]

CONF_IAQ_ACCURACY = "iaq_accuracy"
ICON_ACCURACY = "mdi:checkbox-marked-circle-outline"

TYPES = {CONF_IAQ_ACCURACY: "set_iaq_accuracy_text_sensor"}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BME680_BSEC_ID): cv.use_id(BME680BSECComponent),
        cv.Optional(CONF_IAQ_ACCURACY): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
                cv.Optional(CONF_ICON, default=ICON_ACCURACY): cv.icon,
            }
        ),
    }
)


@coroutine
def setup_conf(config, key, hub, funcName):
    if key in config:
        conf = config[key]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield text_sensor.register_text_sensor(var, conf)
        func = getattr(hub, funcName)
        cg.add(func(var))


def to_code(config):
    hub = yield cg.get_variable(config[CONF_BME680_BSEC_ID])
    for key, funcName in TYPES.items():
        yield setup_conf(config, key, hub, funcName)
