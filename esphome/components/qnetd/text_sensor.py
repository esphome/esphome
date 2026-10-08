import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_STATUS, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_QNETD_ID, Qnetd, qnetd_ns

DEPENDENCIES = ["qnetd"]

QnetdStatusTextSensor = qnetd_ns.class_(
    "QnetdStatusTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_QNETD_ID): cv.use_id(Qnetd),
    cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(
        QnetdStatusTextSensor,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.COMPONENT_SCHEMA),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_QNETD_ID])
    if (conf := config.get(CONF_STATUS)) is not None:
        var = await text_sensor.new_text_sensor(conf, hub)
        await cg.register_component(var, conf)
