import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import cover
from esphome.const import CONF_TILT_ACTION, CONF_ID, CONF_TILT_CLOSED_VALUE, \
  CONF_TILT_OPENED_VALUE, CONF_TILT_CLOSE_SPEED, CONF_TILT_OPEN_SPEED

tilt_ns = cg.esphome_ns.namespace('tilt')
TiltCover = tilt_ns.class_('TiltCover', cover.Cover, cg.Component)


def validate_speed(value):
    value = cv.string(value)
    for suffix in ('%/s', '%/s'):
        if value.endswith(suffix):
            value = value[:-len(suffix)]

    if value == 'inf':
        return 1e6

    try:
        value = float(value)
    except ValueError:
        raise cv.Invalid("Expected speed as floating point number, got {}".format(value))

    if value <= 0:
        raise cv.Invalid("Speed must be larger than 0 %/s!")

    return value


CONFIG_SCHEMA = cover.COVER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(TiltCover),
    cv.Required(CONF_TILT_ACTION): automation.validate_automation(single=True),
    cv.Optional(CONF_TILT_CLOSED_VALUE, default=0): cv.percentage,
    cv.Optional(CONF_TILT_OPENED_VALUE, default=100): cv.percentage,
    cv.Optional(CONF_TILT_CLOSE_SPEED, default='inf'): validate_speed,
    cv.Optional(CONF_TILT_OPEN_SPEED, default='inf'): validate_speed,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield cover.register_cover(var, config)

    yield automation.build_automation(var.get_tilt_trigger(), [], config[CONF_TILT_ACTION])
    cg.add(var.set_tilt_closed_value(config[CONF_TILT_CLOSED_VALUE]))
    cg.add(var.set_tilt_opened_value(config[CONF_TILT_OPENED_VALUE]))
    cg.add(var.set_tilt_close_speed(config[CONF_TILT_CLOSE_SPEED]))
    cg.add(var.set_tilt_open_speed(config[CONF_TILT_OPEN_SPEED]))
