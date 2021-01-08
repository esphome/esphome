import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.components import i2c, time
from esphome.const import CONF_ID


CODEOWNERS = ['@badbadc0ffee']
DEPENDENCIES = ['i2c']
ds1307_ns = cg.esphome_ns.namespace('ds1307')
DS1307Component = ds1307_ns.class_('DS1307Component', time.RealTimeClock, i2c.I2CDevice)
WriteAction = ds1307_ns.class_('WriteAction', automation.Action)
ReadAction = ds1307_ns.class_('ReadAction', automation.Action)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(DS1307Component),
}).extend(i2c.i2c_device_schema(0x68))


@automation.register_action('ds1307.write', WriteAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DS1307Component),
}))
def ds1307_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


@automation.register_action('ds1307.read', ReadAction, automation.maybe_simple_id({
    cv.GenerateID(): cv.use_id(DS1307Component),
}))
def ds1307_read_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    yield time.register_time(var, config)
