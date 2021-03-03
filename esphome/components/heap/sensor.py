import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, ICON_COUNTER, DEVICE_CLASS_EMPTY, CONF_HEAP_FREE, CONF_HEAP_FRAGMENTATION, CONF_HEAP_BLOCK

heap_sensor_ns = cg.esphome_ns.namespace('heap')
HeapSensor = heap_sensor_ns.class_('HeapSensor', cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HeapSensor),
    cv.Optional(CONF_HEAP_FREE): sensor.sensor_schema('Number', ICON_COUNTER, 0, DEVICE_CLASS_EMPTY),
    cv.Optional(CONF_HEAP_FRAGMENTATION): sensor.sensor_schema('Number', ICON_COUNTER, 0, DEVICE_CLASS_EMPTY),
    cv.Optional(CONF_HEAP_BLOCK): sensor.sensor_schema('Number', ICON_COUNTER, 0, DEVICE_CLASS_EMPTY),
}).extend(cv.polling_component_schema('60s'))

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    if CONF_HEAP_FREE in config:
        sens = yield sensor.new_sensor(config[CONF_HEAP_FREE])
        cg.add(var.set_free_sensor(sens))

    if CONF_HEAP_FRAGMENTATION in config:
        sens = yield sensor.new_sensor(config[CONF_HEAP_FRAGMENTATION])
        cg.add(var.set_fragmentation_sensor(sens))

    if CONF_HEAP_BLOCK in config:
        sens = yield sensor.new_sensor(config[CONF_HEAP_BLOCK])
        cg.add(var.set_block_sensor(sens))
