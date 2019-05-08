import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import time
from esphome.const import CONF_TIME_ID, CONF_ID, CONF_TRIGGER_ID

sun_ns = cg.esphome_ns.namespace('sun')

Sun = sun_ns.class_('Sun')
SunTrigger = sun_ns.class_('SunTrigger', cg.PollingComponent, automation.Trigger.template())

CONF_SUN_ID = 'sun_id'
CONF_LATITUDE = 'latitude'
CONF_LONGITUDE = 'longitude'
CONF_ELEVATION = 'elevation'
CONF_ON_SUNRISE = 'on_sunrise'
CONF_ON_SUNSET = 'on_sunset'

elevation = cv.All(cv.angle, cv.float_range(min=-180, max=180))

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Sun),
    cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
    cv.Required(CONF_LATITUDE): cv.float_range(min=-90, max=90),
    cv.Required(CONF_LONGITUDE): cv.float_range(min=-180, max=180),

    cv.Optional(CONF_ON_SUNRISE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SunTrigger),
        cv.Optional(CONF_ELEVATION, default=0.0): elevation,
    }),
    cv.Optional(CONF_ON_SUNSET): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SunTrigger),
        cv.Optional(CONF_ELEVATION, default=0.0): elevation,
    }),
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    time_ = yield cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))
    cg.add(var.set_latitude(config[CONF_LATITUDE]))
    cg.add(var.set_longitude(config[CONF_LONGITUDE]))

    for conf in config.get(CONF_ON_SUNRISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield cg.register_component(trigger, conf)
        cg.add(trigger.set_sunrise(True))
        cg.add(trigger.set_elevation(conf[CONF_ELEVATION]))
        yield automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_SUNSET, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield cg.register_component(trigger, conf)
        cg.add(trigger.set_sunrise(False))
        cg.add(trigger.set_elevation(conf[CONF_ELEVATION]))
        yield automation.build_automation(trigger, [], conf)
