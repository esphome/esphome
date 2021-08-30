import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from .. import CONF_PIPSOLAR_ID, PIPSOLAR_COMPONENT_SCHEMA, pipsolar_ns

DEPENDENCIES = ["uart"]

CONF_DEVICE_MODE = "device_mode"
CONF_LAST_QPIGS = "last_qpigs"
CONF_LAST_QPIRI = "last_qpiri"
CONF_LAST_QMOD = "last_qmod"
CONF_LAST_QFLAG = "last_qflag"
CONF_LAST_QPIWS = "last_qpiws"
CONF_LAST_QT = "last_qt"
CONF_LAST_QMN = "last_qmn"

PipsolarTextSensor = pipsolar_ns.class_(
    "PipsolarTextSensor", text_sensor.TextSensor, cg.Component
)

TYPES = [
    CONF_DEVICE_MODE,
    CONF_LAST_QPIGS,
    CONF_LAST_QPIRI,
    CONF_LAST_QMOD,
    CONF_LAST_QFLAG,
    CONF_LAST_QPIWS,
    CONF_LAST_QT,
    CONF_LAST_QMN,
]

CONFIG_SCHEMA = PIPSOLAR_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(type): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {cv.GenerateID(): cv.declare_id(PipsolarTextSensor)}
        )
        for type in TYPES
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PIPSOLAR_ID])

    for type in TYPES:
        if type in config:
            conf = config[type]
            var = cg.new_Pvariable(conf[CONF_ID])
            await text_sensor.register_text_sensor(var, conf)
            await cg.register_component(var, conf)
            cg.add(getattr(paren, f"set_{type}")(var))
