import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, simpleevse
from esphome.const import CONF_ID, UNIT_EMPTY, UNIT_AMPERE, ICON_EMPTY, DEVICE_CLASS_CURRENT, DEVICE_CLASS_EMPTY
from . import simpleevse_ns

DEPENDENCIES = ['simpleevse']
AUTO_LOAD = ['sensor']

CONF_SIMPLEEVSE_ID = 'simpleevse_id'

CONF_SET_CHARGE_CURRENT = "set_charge_current"
CONF_ACTUAL_CHARGE_CURRENT = 'actual_charge_current'
CONF_MAX_CURRENT_LIMIT = 'max_current_limit'
CONF_FIRMWARE_REVISION = 'firmware_revision'

SimpleEvseSensors =  simpleevse_ns.class_('SimpleEvseSensors')

# Schema for sensors
set_charge_current_schema = sensor.sensor_schema(UNIT_AMPERE, ICON_EMPTY, 0, DEVICE_CLASS_CURRENT)
actual_charge_current_schema = sensor.sensor_schema(UNIT_AMPERE, ICON_EMPTY, 0, DEVICE_CLASS_CURRENT)
max_current_limit_schema = sensor.sensor_schema(UNIT_AMPERE, ICON_EMPTY, 0, DEVICE_CLASS_CURRENT)
firmware_revision_schema = sensor.sensor_schema(UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SimpleEvseSensors),
    cv.GenerateID(CONF_SIMPLEEVSE_ID): cv.use_id(simpleevse.SimpleEvseComponent),
    cv.Optional(CONF_SET_CHARGE_CURRENT): set_charge_current_schema,
    cv.Optional(CONF_ACTUAL_CHARGE_CURRENT): actual_charge_current_schema,
    cv.Optional(CONF_MAX_CURRENT_LIMIT): max_current_limit_schema,
    cv.Optional(CONF_FIRMWARE_REVISION): firmware_revision_schema,
})

def to_code(config):
    evse = yield cg.get_variable(config[CONF_SIMPLEEVSE_ID])
    var = cg.new_Pvariable(config[CONF_ID], evse)
        
    if CONF_SET_CHARGE_CURRENT in config:
        sens = yield sensor.new_sensor(config[CONF_SET_CHARGE_CURRENT])
        cg.add(var.set_set_charge_current_sensor(sens))

    if CONF_ACTUAL_CHARGE_CURRENT in config:
        sens = yield sensor.new_sensor(config[CONF_ACTUAL_CHARGE_CURRENT])
        cg.add(var.set_actual_charge_current_sensor(sens))

    if CONF_MAX_CURRENT_LIMIT in config:
        sens = yield sensor.new_sensor(config[CONF_MAX_CURRENT_LIMIT])
        cg.add(var.set_max_current_limit_sensor(sens))

    if CONF_FIRMWARE_REVISION in config:
        sens = yield sensor.new_sensor(config[CONF_FIRMWARE_REVISION])
        cg.add(var.set_firmware_revision_sensor(sens))
