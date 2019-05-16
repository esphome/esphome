import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import cover
from esphome.const import CONF_TILT_ACTION, CONF_ID, CONF_TILT_CLOSED_VALUE, CONF_TILT_OPENED_VALUE

tilt_ns = cg.esphome_ns.namespace('tilt')
TiltCover = tilt_ns.class_('TiltCover', cover.Cover, cg.Component)

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(TiltCover),
    cv.Required(CONF_TILT_ACTION): automation.validate_automation(single=True),
    cv.Optional(CONF_TILT_CLOSED_VALUE, default=0): cv.percentage,
    cv.Optional(CONF_TILT_OPENED_VALUE, default=100): cv.percentage,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield cover.register_cover(var, config)

    yield automation.build_automation(var.get_tilt_trigger(), [], config[CONF_TILT_ACTION])
    cg.add(var.set_tilt_closed_value(config[CONF_TILT_CLOSED_VALUE]))
    cg.add(var.set_tilt_opened_value(config[CONF_TILT_OPENED_VALUE]))
