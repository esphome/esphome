import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_ICON, ICON_NEW_BOX

version_ns = cg.esphome_ns.namespace('version')
VersionTextSensor = version_ns.class_('VersionTextSensor', text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(VersionTextSensor),
    cv.Optional(CONF_ICON, default=ICON_NEW_BOX): text_sensor.icon
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield text_sensor.register_text_sensor(var, config)
    yield cg.register_component(var, config)
