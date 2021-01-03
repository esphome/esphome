import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.components import i2c, time
from esphome.const import CONF_ID


CODEOWNERS = ['@badbadc0ffee']
DEPENDENCIES = ['i2c']
ds1307_ns = cg.esphome_ns.namespace('ds1307')
DS1307Component = ds1307_ns.class_('DS1307Component', time.RealTimeClock, i2c.I2CDevice)
SyncToRtcAction = ds1307_ns.class_('SyncToRtcAction', automation.Action)
SyncFromRtcAction = ds1307_ns.class_('SyncFromRtcAction', automation.Action)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(DS1307Component),
}).extend(i2c.i2c_device_schema(0x68))


@automation.register_action('ds1307.sync_to_rtc', SyncToRtcAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DS1307Component),
}))
def ds1307_sync_to_rtc_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


@automation.register_action('ds1307.sync_from_rtc', SyncFromRtcAction, cv.Schema({
    cv.GenerateID(): cv.use_id(DS1307Component),
}))
def ds1307_sync_from_rtc_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    yield var


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    yield time.register_time(var, config)
