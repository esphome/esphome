import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
)
from esphome.types import ConfigType

from . import CONF_QNETD_ID, Qnetd, qnetd_ns

CONF_CONNECTED_CLIENTS = "connected_clients"
CONF_DECISIONS = "decisions"

DEPENDENCIES = ["qnetd"]

QnetdSensor = qnetd_ns.class_("QnetdSensor", sensor.Sensor, cg.Component)
QnetdSensorType = qnetd_ns.enum("QnetdSensorType", is_class=True)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_QNETD_ID): cv.use_id(Qnetd),
    cv.Optional(CONF_CONNECTED_CLIENTS): sensor.sensor_schema(
        QnetdSensor,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.COMPONENT_SCHEMA),
    cv.Optional(CONF_DECISIONS): sensor.sensor_schema(
        QnetdSensor,
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(cv.COMPONENT_SCHEMA),
}

SENSOR_TYPES = {
    CONF_CONNECTED_CLIENTS: QnetdSensorType.QNETD_SENSOR_TYPE_CONNECTED_CLIENTS,
    CONF_DECISIONS: QnetdSensorType.QNETD_SENSOR_TYPE_DECISIONS,
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_QNETD_ID])
    for key, sensor_type in SENSOR_TYPES.items():
        if (conf := config.get(key)) is not None:
            var = await sensor.new_sensor(conf, hub, sensor_type)
            await cg.register_component(var, conf)
