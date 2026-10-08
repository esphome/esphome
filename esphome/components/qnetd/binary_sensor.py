import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_QNETD_ID, Qnetd, qnetd_ns

CONF_VOTE_GRANTED = "vote_granted"

DEPENDENCIES = ["qnetd"]

QnetdVoteGrantedBinarySensor = qnetd_ns.class_(
    "QnetdVoteGrantedBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_QNETD_ID): cv.use_id(Qnetd),
    cv.Optional(CONF_VOTE_GRANTED): binary_sensor.binary_sensor_schema(
        QnetdVoteGrantedBinarySensor,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.COMPONENT_SCHEMA),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_QNETD_ID])
    if (conf := config.get(CONF_VOTE_GRANTED)) is not None:
        var = await binary_sensor.new_binary_sensor(conf, hub)
        await cg.register_component(var, conf)
