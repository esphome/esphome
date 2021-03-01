import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, modbus
from esphome.const import CONF_ID, CONF_LENGTH, CONF_SENSORS, DEVICE_CLASS_EMPTY, ICON_EMPTY, UNIT_EMPTY


AUTO_LOAD = ['modbus']

CONF_REGISTER = 'register'
CONF_REVERSE_ORDER = 'reverse_order'

modbus_sensor_ns = cg.esphome_ns.namespace('modbus_sensor')
ModbusSensor = modbus_sensor_ns.class_('ModbusSensor', cg.PollingComponent, modbus.ModbusDevice)


REGISTER_SCHEMA = cv.Schema({
    cv.Optional(CONF_LENGTH, default=1): cv.int_range(min=1, max=2),
    cv.Optional(CONF_REVERSE_ORDER, default=False): cv.boolean,
}).extend(sensor.sensor_schema(UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY))


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ModbusSensor),
    cv.Required(CONF_REGISTER): cv.hex_uint16_t,
    cv.Required(CONF_SENSORS): cv.All(cv.ensure_list(REGISTER_SCHEMA), cv.Length(min=1)),
}).extend(cv.polling_component_schema('60s')).extend(modbus.modbus_device_schema(0x01))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield modbus.register_modbus_device(var, config)
    cg.add(var.set_register(config[CONF_REGISTER]))
    register_count = 0
    for sensor_config in config[CONF_SENSORS]:
        register_count += sensor_config[CONF_LENGTH]
        sens = yield sensor.new_sensor(sensor_config)
        cg.add(var.set_sensor(sens))
        cg.add(var.set_sensor_length(sensor_config[CONF_LENGTH]))
        cg.add(var.set_sensor_reverse_order(sensor_config[CONF_REVERSE_ORDER]))
    cg.add(var.set_register_count(register_count))
