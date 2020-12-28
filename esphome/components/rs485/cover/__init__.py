import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_DATA, CONF_ID, CONF_INVERTED
from esphome.core import HexInt
from .. import rs485_ns, CONF_RS485_ID, RS485, validate_raw_data

CODEOWNERS = ['@loongyh']
DEPENDENCIES = ['rs485']

RS485Cover = rs485_ns.class_('RS485Cover', cover.Cover, cg.Component)

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(RS485Cover),
    cv.GenerateID(CONF_RS485_ID): cv.use_id(RS485),
    cv.Optional(CONF_HEADER): sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_ADDRESS): sensor.sensor_schema(UNIT_AMPERE, ICON_CURRENT_AC, 3),
    cv.Optional(CONF_LENGTH): sensor.sensor_schema(UNIT_WATT, ICON_POWER, 1),
    cv.Optional(CONF_COMMAND): sensor.sensor_schema(UNIT_WATT_HOURS, ICON_COUNTER, 0),
    cv.Optional(CONF_PAYLOAD): sensor.sensor_schema(UNIT_HERTZ, ICON_CURRENT_AC, 1),
    cv.Optional(CONF_CHECKSUM): sensor.sensor_schema(UNIT_EMPTY, ICON_FLASH, 2),
}).extend(rs485.rs485_device_schema(0x01))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield rs485.register_rs485_device(var, config)

    if CONF_VOLTAGE in config:
        conf = config[CONF_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_voltage_sensor(sens))
    if CONF_CURRENT in config:
        conf = config[CONF_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_current_sensor(sens))
    if CONF_POWER in config:
        conf = config[CONF_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_power_sensor(sens))
    if CONF_ENERGY in config:
        conf = config[CONF_ENERGY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_energy_sensor(sens))
    if CONF_FREQUENCY in config:
        conf = config[CONF_FREQUENCY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_frequency_sensor(sens))
    if CONF_POWER_FACTOR in config:
        conf = config[CONF_POWER_FACTOR]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_power_factor_sensor(sens))
