import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, i2c
from esphome.const import CONF_ID, CONF_VOLTAGE, \
    UNIT_VOLT, ICON_FLASH, UNIT_AMPERE, UNIT_WATT

DEPENDENCIES = ['i2c']

ace7953_ns = cg.esphome_ns.namespace('ade7953')
ADE7953 = ace7953_ns.class_('ADE7953', cg.PollingComponent, i2c.I2CDevice)

CONF_CURRENT_A = 'current_a'
CONF_CURRENT_B = 'current_b'
CONF_ACTIVE_POWER_A = 'active_power_a'
CONF_ACTIVE_POWER_B = 'active_power_b'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ADE7953),

    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_CURRENT_A): sensor.sensor_schema(UNIT_AMPERE, ICON_FLASH, 2),
    cv.Optional(CONF_CURRENT_B): sensor.sensor_schema(UNIT_AMPERE, ICON_FLASH, 2),
    cv.Optional(CONF_ACTIVE_POWER_A): sensor.sensor_schema(UNIT_WATT, ICON_FLASH, 1),
    cv.Optional(CONF_ACTIVE_POWER_B): sensor.sensor_schema(UNIT_WATT, ICON_FLASH, 1),
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x38))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    for key in [CONF_VOLTAGE, CONF_CURRENT_A, CONF_CURRENT_B, CONF_ACTIVE_POWER_A,
                CONF_ACTIVE_POWER_B]:
        if key not in config:
            continue
        conf = config[key]
        sens = yield sensor.new_sensor(conf)
        cg.add(getattr(var, 'set_{}_sensor'.format(key))(sens))
