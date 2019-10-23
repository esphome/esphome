import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import sensor, binary_sensor
from esphome.const import CONF_ID, CONF_CHANNELS, CONF_VALUE, CONF_TYPE, UNIT_EMPTY, \
    ICON_CHECK_CIRCLE_OUTLINE, CONF_BINARY_SENSOR, CONF_GROUP

DEPENDENCIES = ['binary_sensor']

binary_sensor_map_ns = cg.esphome_ns.namespace('binary_sensor_map')
BinarySensorMap = binary_sensor_map_ns.class_('BinarySensorMap', cg.Component, sensor.Sensor)
SensorMapType = binary_sensor_map_ns.enum('SensorMapType')

SENSOR_MAP_TYPES = {
    CONF_GROUP: SensorMapType.BINARY_SENSOR_MAP_TYPE_GROUP,
}

entry = {
    cv.Required(CONF_BINARY_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    cv.Required(CONF_VALUE): cv.float_,
}

CONFIG_SCHEMA = cv.typed_schema({
    CONF_GROUP: sensor.sensor_schema(UNIT_EMPTY, ICON_CHECK_CIRCLE_OUTLINE, 0).extend({
        cv.GenerateID(): cv.declare_id(BinarySensorMap),
        cv.Required(CONF_CHANNELS): cv.All(cv.ensure_list(entry), cv.Length(min=1)),
    }),
}, lower=True)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    constant = SENSOR_MAP_TYPES[config[CONF_TYPE]]
    cg.add(var.set_sensor_type(constant))

    for ch in config[CONF_CHANNELS]:
        input_var = yield cg.get_variable(ch[CONF_BINARY_SENSOR])
        cg.add(var.add_channel(input_var, ch[CONF_VALUE]))
