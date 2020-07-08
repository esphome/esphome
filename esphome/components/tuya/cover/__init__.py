from esphome.components import cover
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_OUTPUT_ID, CONF_OPTIMISTIC, CONF_RESTORE_MODE
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ['tuya']

CONF_POSITION_REPORT_DATAPOINT = "position_report_datapoint"
CONF_POSITION_CONTROL_DATAPOINT = "position_control_datapoint"
CONF_CONTROL_DATAPOINT = "control_datapoint"
CONF_DIRECTION_DATAPOINT = "direction_datapoint"

TuyaCover = tuya_ns.class_('TuyaCover', cover.Cover, cg.Component)

TuyaCoverRestoreMode = tuya_ns.enum('TuyaCoverRestoreMode')
RESTORE_MODES = {
    'NO_RESTORE': TuyaCoverRestoreMode.COVER_NO_RESTORE,
    'RESTORE': TuyaCoverRestoreMode.COVER_RESTORE,
    'RESTORE_AND_CALL': TuyaCoverRestoreMode.COVER_RESTORE_AND_CALL,
}

CONFIG_SCHEMA = cv.All(cover.COVER_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TuyaCover),
    cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
    cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
    cv.Optional(CONF_POSITION_REPORT_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_POSITION_CONTROL_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_CONTROL_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_DIRECTION_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_RESTORE_MODE, default='RESTORE'): cv.enum(RESTORE_MODES, upper=True),
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    yield cg.register_component(var, config)

    paren = yield cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))
    yield cover.register_cover(var, config)

    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if CONF_POSITION_REPORT_DATAPOINT in config:
        cg.add(var.set_position_report_id(config[CONF_POSITION_REPORT_DATAPOINT]))
    if CONF_POSITION_CONTROL_DATAPOINT in config:
        cg.add(var.set_position_control_id(config[CONF_POSITION_CONTROL_DATAPOINT]))
    if CONF_CONTROL_DATAPOINT in config:
        cg.add(var.set_control_id(config[CONF_CONTROL_DATAPOINT]))
    if CONF_DIRECTION_DATAPOINT in config:
        cg.add(var.set_direction_id(config[CONF_DIRECTION_DATAPOINT]))
