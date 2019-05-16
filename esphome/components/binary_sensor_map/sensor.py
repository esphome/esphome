import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import sensor, binary_sensor
from esphome.const import CONF_ID, CONF_NAME, CONF_CHANNELS, CONF_CHANNEL, CONF_VALUE, CONF_TYPE

DEPENDENCIES = ['binary_sensor']

binary_sensor_map_ns = cg.esphome_ns.namespace('binary_sensor_map')
BinarySensorMap = binary_sensor_map_ns.class_('BinarySensorMap', cg.Component)
SensorMapType = binary_sensor_map_ns.enum('SensorMapType')
SENSOR_MAP_TYPES = {
    'GROUP': SensorMapType.BINARY_SENSOR_MAP_TYPE_GROUP,
}

entry = {
    cv.Required(CONF_CHANNEL): cv.use_id(binary_sensor.BinarySensor),
    cv.Required(CONF_VALUE): cv.All(cv.positive_int, cv.Range(min=0, max=255)),
}

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BinarySensorMap),
    cv.Required(CONF_TYPE): cv.one_of(*SENSOR_MAP_TYPES, upper=True),
    cv.Required(CONF_CHANNELS): cv.ensure_list(entry),
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
    if CONF_TYPE in config:
        constant = SENSOR_MAP_TYPES[config[CONF_TYPE]]
        cg.add(var.set_sensor_type(constant))

    for ch in config[CONF_CHANNELS]:
        input_var = yield cg.get_variable(ch[CONF_CHANNEL])
        cg.add(var.add_sensor(input_var,ch[CONF_VALUE]))