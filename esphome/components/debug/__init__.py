import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import sensor
from esphome.const import CONF_ID, ICON_COUNTER, DEVICE_CLASS_EMPTY, \
    CONF_FREE, CONF_FRAGMENTATION, CONF_BLOCK
import esphome.core_config as cc

CODEOWNERS = ['@OttoWinter']
DEPENDENCIES = ['logger']

debug_ns = cg.esphome_ns.namespace('debug')
DebugComponent = debug_ns.class_('DebugComponent', cg.PollingComponent)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DebugComponent),
    cv.Optional(CONF_FREE): sensor.sensor_schema('Number', ICON_COUNTER, 0, DEVICE_CLASS_EMPTY),
    cv.Optional(CONF_FRAGMENTATION): cv.All(
        cc.atleast_esp8266_framework('2.5.2'),
        cv.only_on_esp8266,
        sensor.sensor_schema('Number', ICON_COUNTER, 0, DEVICE_CLASS_EMPTY)
    ),
    cv.Optional(CONF_BLOCK): cv.All(
        cc.atleast_esp8266_framework('2.5.2'),
        cv.only_on_esp8266,
        sensor.sensor_schema('Number', ICON_COUNTER, 0, DEVICE_CLASS_EMPTY)
    ),
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    if CONF_FREE in config:
        sens = yield sensor.new_sensor(config[CONF_FREE])
        cg.add(var.set_free_sensor(sens))

    if CONF_FRAGMENTATION in config:
        sens = yield sensor.new_sensor(config[CONF_FRAGMENTATION])
        cg.add(var.set_fragmentation_sensor(sens))

    if CONF_BLOCK in config:
        sens = yield sensor.new_sensor(config[CONF_BLOCK])
        cg.add(var.set_block_sensor(sens))
