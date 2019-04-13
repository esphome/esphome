import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_UPDATE_INTERVAL, CONF_I2C_ID

DEPENDENCIES = ['i2c']
AUTO_LOAD = ['sensor', 'binary_sensor']
MULTI_CONF = True

CONF_APDS9960_ID = 'apds9960_id'

apds9960_nds = cg.esphome_ns.namespace('apds9960')
APDS9960 = apds9960_nds.class_('APDS9960', cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(APDS9960),
    cv.GenerateID(CONF_I2C_ID): cv.declare_variable_id(i2c.I2CComponent),

    cv.Optional(CONF_ADDRESS, default=0x39): cv.i2c_address,
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    i2c_ = yield cg.get_variable(config[CONF_I2C_ID])
    var = cg.new_Pvariable(config[CONF_ID], i2c_, config[CONF_ADDRESS],
                           config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)
