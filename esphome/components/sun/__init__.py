import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import time
from esphome.const import CONF_TIME_ID, CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ['@OttoWinter']
sun_ns = cg.esphome_ns.namespace('sun')

Sun = sun_ns.class_('Sun')
SunTrigger = sun_ns.class_('SunTrigger', cg.PollingComponent, automation.Trigger.template())
SunCondition = sun_ns.class_('SunCondition', automation.Condition)

CONF_SUN_ID = 'sun_id'
CONF_LATITUDE = 'latitude'
CONF_LONGITUDE = 'longitude'
CONF_ELEVATION = 'elevation'
CONF_ON_SUNRISE = 'on_sunrise'
CONF_ON_SUNSET = 'on_sunset'

# Default sun elevation is a bit below horizon because sunset
# means time when the entire sun disk is below the horizon
DEFAULT_ELEVATION = -0.883

ELEVATION_MAP = {
    'sunrise': 0.0,
    'sunset': 0.0,
    'civil': -6.0,
    'nautical': -12.0,
    'astronomical': -18.0,
}


def elevation(value):
    if isinstance(value, str):
        try:
            value = ELEVATION_MAP[cv.one_of(*ELEVATION_MAP, lower=True, space='_')(value)]
        except cv.Invalid:
            pass
    value = cv.angle(value)
    return cv.float_range(min=-180, max=180)(value)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Sun),
    cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
    cv.Required(CONF_LATITUDE): cv.float_range(min=-90, max=90),
    cv.Required(CONF_LONGITUDE): cv.float_range(min=-180, max=180),

    cv.Optional(CONF_ON_SUNRISE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SunTrigger),
        cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): elevation,
    }),
    cv.Optional(CONF_ON_SUNSET): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SunTrigger),
        cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): elevation,
    }),
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    time_ = yield cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))
    cg.add(var.set_latitude(config[CONF_LATITUDE]))
    cg.add(var.set_longitude(config[CONF_LONGITUDE]))

    for conf in config.get(CONF_ON_SUNRISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        yield cg.register_component(trigger, conf)
        yield cg.register_parented(trigger, var)
        cg.add(trigger.set_sunrise(True))
        cg.add(trigger.set_elevation(conf[CONF_ELEVATION]))
        yield automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_SUNSET, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        yield cg.register_component(trigger, conf)
        yield cg.register_parented(trigger, var)
        cg.add(trigger.set_sunrise(False))
        cg.add(trigger.set_elevation(conf[CONF_ELEVATION]))
        yield automation.build_automation(trigger, [], conf)


@automation.register_condition('sun.is_above_horizon', SunCondition, cv.Schema({
    cv.GenerateID(): cv.use_id(Sun),
    cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): cv.templatable(elevation),
}))
def sun_above_horizon_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    templ = yield cg.templatable(config[CONF_ELEVATION], args, cg.double)
    cg.add(var.set_elevation(templ))
    cg.add(var.set_above(True))
    yield var


@automation.register_condition('sun.is_below_horizon', SunCondition, cv.Schema({
    cv.GenerateID(): cv.use_id(Sun),
    cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): cv.templatable(elevation),
}))
def sun_below_horizon_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    templ = yield cg.templatable(config[CONF_ELEVATION], args, cg.double)
    cg.add(var.set_elevation(templ))
    cg.add(var.set_above(False))
    yield var
