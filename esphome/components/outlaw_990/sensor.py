import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import UNIT_DECIBEL
from . import CONF_OUTLAW_990_ID, Outlaw990

DEPENDENCIES = ["outlaw_990"]

CONF_SENSOR_VOLUME = "sensor_volume"

TYPES = [
    CONF_SENSOR_VOLUME,
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OUTLAW_990_ID): cv.use_id(Outlaw990),
        cv.Required(CONF_SENSOR_VOLUME): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL, accuracy_decimals=0
        ),
    }
)


async def to_code(config):
    comp = await cg.get_variable(config[CONF_OUTLAW_990_ID])

    if CONF_SENSOR_VOLUME in config:
        conf = config[CONF_SENSOR_VOLUME]
        sens = await sensor.new_sensor(conf)
        cg.add(comp.set_volume_sensor(sens))
