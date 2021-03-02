import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, modbus
from esphome.const import CONF_ID, CONF_SENSORS, DEVICE_CLASS_EMPTY, ICON_EMPTY, UNIT_EMPTY


modbus_sensor_ns = cg.esphome_ns.namespace('modbus_sensor')
ModbusSensor = modbus_sensor_ns.class_('ModbusSensor', cg.PollingComponent, modbus.ModbusDevice)

AUTO_LOAD = ['modbus']

CONF_REGISTER_ADDRESS = 'register_address'
CONF_REGISTER_TYPE = 'register_type'
CONF_REVERSE_ORDER = 'reverse_order'


RegisterType = modbus_sensor_ns.enum('RegisterType')
REGISTER_TYPES = {
    '16bit': RegisterType.REGISTER_TYPE_16BIT,
    '32bit': RegisterType.REGISTER_TYPE_32BIT,
    '32bit_reversed': RegisterType.REGISTER_TYPE_32BIT_REVERSED,
}


REGISTER_SCHEMA = cv.Schema({
    cv.Optional(CONF_REGISTER_TYPE, default='16bit'): cv.enum(REGISTER_TYPES),
}).extend(sensor.sensor_schema(UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY))


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ModbusSensor),
    cv.Required(CONF_REGISTER_ADDRESS): cv.hex_uint16_t,
    cv.Required(CONF_SENSORS): cv.All(cv.ensure_list(REGISTER_SCHEMA), cv.Length(min=1)),
}).extend(cv.polling_component_schema('60s')).extend(modbus.modbus_device_schema(0x01))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield modbus.register_modbus_device(var, config)
    cg.add(var.set_register_address(config[CONF_REGISTER_ADDRESS]))
    for sensor_config in config[CONF_SENSORS]:
        sens = yield sensor.new_sensor(sensor_config)
        cg.add(var.set_sensor(sens, sensor_config[CONF_REGISTER_TYPE]))
