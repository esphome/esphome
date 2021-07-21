import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_PIPSOLAR_ID
from . import PipsolarComponent, pipsolar_ns

DEPENDENCIES = ["uart"]

CONF_DEVICE_MODE = "device_mode"
CONF_LAST_QPIGS = "last_qpigs"
CONF_LAST_QPIRI = "last_qpiri"
CONF_LAST_QMOD = "last_qmod"
CONF_LAST_QFLAG = "last_qflag"
CONF_LAST_QPIWS = "last_qpiws"
CONF_LAST_QT = "last_qt"
CONF_LAST_QMN = "last_qmn"

pipsolar_text_sensor_ns = pipsolar_ns.class_(
    "PipsolarTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.Required(CONF_PIPSOLAR_ID): cv.use_id(PipsolarComponent),
        cv.Optional(CONF_DEVICE_MODE): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(pipsolar_text_sensor_ns),
            }
        ),
        cv.Optional(CONF_LAST_QPIGS): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(pipsolar_text_sensor_ns),
            }
        ),
        cv.Optional(CONF_LAST_QPIRI): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(pipsolar_text_sensor_ns),
            }
        ),
        cv.Optional(CONF_LAST_QMOD): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(pipsolar_text_sensor_ns),
            }
        ),
        cv.Optional(CONF_LAST_QFLAG): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(pipsolar_text_sensor_ns),
            }
        ),
        cv.Optional(CONF_LAST_QPIWS): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(pipsolar_text_sensor_ns),
            }
        ),
        cv.Optional(CONF_LAST_QT): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(pipsolar_text_sensor_ns),
            }
        ),
        cv.Optional(CONF_LAST_QMN): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(pipsolar_text_sensor_ns),
            }
        ),
    }
)


def to_code(config):
    paren = yield cg.get_variable(config[CONF_PIPSOLAR_ID])
    if CONF_DEVICE_MODE in config:
        conf = config[CONF_DEVICE_MODE]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield text_sensor.register_text_sensor(var, conf)
        yield cg.register_component(var, conf)
        cg.add(paren.set_device_mode_sensor(var))
    if CONF_LAST_QPIGS in config:
        conf = config[CONF_LAST_QPIGS]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield text_sensor.register_text_sensor(var, conf)
        yield cg.register_component(var, conf)
        cg.add(paren.set_last_qpigs_sensor(var))
    if CONF_LAST_QPIRI in config:
        conf = config[CONF_LAST_QPIRI]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield text_sensor.register_text_sensor(var, conf)
        yield cg.register_component(var, conf)
        cg.add(paren.set_last_qpiri_sensor(var))
    if CONF_LAST_QMOD in config:
        conf = config[CONF_LAST_QMOD]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield text_sensor.register_text_sensor(var, conf)
        yield cg.register_component(var, conf)
        cg.add(paren.set_last_qmod_sensor(var))
    if CONF_LAST_QFLAG in config:
        conf = config[CONF_LAST_QFLAG]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield text_sensor.register_text_sensor(var, conf)
        yield cg.register_component(var, conf)
        cg.add(paren.set_last_qflag_sensor(var))
    if CONF_LAST_QPIWS in config:
        conf = config[CONF_LAST_QPIWS]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield text_sensor.register_text_sensor(var, conf)
        yield cg.register_component(var, conf)
        cg.add(paren.set_last_qpiws_sensor(var))
    if CONF_LAST_QT in config:
        conf = config[CONF_LAST_QT]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield text_sensor.register_text_sensor(var, conf)
        yield cg.register_component(var, conf)
        cg.add(paren.set_last_qt_sensor(var))
    if CONF_LAST_QMN in config:
        conf = config[CONF_LAST_QMN]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield text_sensor.register_text_sensor(var, conf)
        yield cg.register_component(var, conf)
        cg.add(paren.set_last_qmn_sensor(var))
