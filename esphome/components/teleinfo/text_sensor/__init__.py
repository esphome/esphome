import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.const import CONF_ID

from .. import CONF_TAG_NAME, TELEINFO_LISTENER_SCHEMA, teleinfo_ns, CONF_TELEINFO_ID

TeleInfoTextSensor = teleinfo_ns.class_(
    "TeleInfoTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(TeleInfoTextSensor).extend(
    TELEINFO_LISTENER_SCHEMA
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)
    teleinfo = await cg.get_variable(config[CONF_TELEINFO_ID])
    cg.add(teleinfo.register_teleinfo_listener(var))
