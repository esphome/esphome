import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import CONF_C4002_ID, C4002Component, dfrobot_c4002_ns

C4002TextSensorHub = dfrobot_c4002_ns.class_("C4002TextSensorHub", cg.Component)

C4002_TEXT_SENSOR = "c4002_text_sensor"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4002_ID): cv.use_id(C4002Component),
        # 这里定义一个“HA 里可见的 text_sensor”
        cv.Optional(C4002_TEXT_SENSOR): text_sensor.text_sensor_schema(
            icon="mdi:message-text-outline"
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_C4002_ID])

    if C4002_TEXT_SENSOR in config:
        ts = await text_sensor.new_text_sensor(config[C4002_TEXT_SENSOR])
        cg.add(parent.set_text_sensor(ts))
