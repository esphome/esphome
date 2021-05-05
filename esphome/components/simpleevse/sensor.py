import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor, binary_sensor, simpleevse
from esphome.const import CONF_ID, UNIT_EMPTY, UNIT_AMPERE, ICON_EMPTY, CONF_DEVICE_CLASS, DEVICE_CLASS_CURRENT, DEVICE_CLASS_EMPTY
from . import simpleevse_ns

DEPENDENCIES = ['simpleevse']
AUTO_LOAD = ['sensor', 'text_sensor', 'binary_sensor']

CONF_SIMPLEEVSE_ID = 'simpleevse_id'

CONF_CONNECTED = "connected"
CONF_SET_CHARGE_CURRENT = "set_charge_current"
CONF_ACTUAL_CHARGE_CURRENT = 'actual_charge_current'
CONF_VEHICLE_STATE = 'vehicle_state'
CONF_MAX_CURRENT_LIMIT = 'max_current_limit'
CONF_FIRMWARE_REVISION = 'firmware_revision'
CONF_EVSE_STATE = 'evse_state'

SimpleEvseSensors =  simpleevse_ns.class_('SimpleEvseSensors')

# Schema for sensors
connected_schema = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.Optional(CONF_DEVICE_CLASS, default='connectivity'): binary_sensor.device_class,
})
set_charge_current_schema = sensor.sensor_schema(UNIT_AMPERE, ICON_EMPTY, 0, DEVICE_CLASS_CURRENT)
actual_charge_current_schema = sensor.sensor_schema(UNIT_AMPERE, ICON_EMPTY, 0, DEVICE_CLASS_CURRENT)
vehicle_state_schema = text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
})
max_current_limit_schema = sensor.sensor_schema(UNIT_AMPERE, ICON_EMPTY, 0, DEVICE_CLASS_CURRENT)
firmware_revision_schema = sensor.sensor_schema(UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY)
evse_state_schema = text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
})


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SimpleEvseSensors),
    cv.GenerateID(CONF_SIMPLEEVSE_ID): cv.use_id(simpleevse.SimpleEvseComponent),
    cv.Optional(CONF_CONNECTED): connected_schema,
    cv.Optional(CONF_SET_CHARGE_CURRENT): set_charge_current_schema,
    cv.Optional(CONF_ACTUAL_CHARGE_CURRENT): actual_charge_current_schema,
    cv.Optional(CONF_VEHICLE_STATE): vehicle_state_schema,
    cv.Optional(CONF_MAX_CURRENT_LIMIT): max_current_limit_schema,
    cv.Optional(CONF_FIRMWARE_REVISION): firmware_revision_schema,
    cv.Optional(CONF_EVSE_STATE): evse_state_schema,
})

def to_code(config):
    evse = yield cg.get_variable(config[CONF_SIMPLEEVSE_ID])
    var = cg.new_Pvariable(config[CONF_ID], evse)

    if CONF_CONNECTED in config:
        sens = yield binary_sensor.new_binary_sensor(config[CONF_CONNECTED])
        cg.add(var.set_connected_sensor(sens))
        
    if CONF_SET_CHARGE_CURRENT in config:
        sens = yield sensor.new_sensor(config[CONF_SET_CHARGE_CURRENT])
        cg.add(var.set_set_charge_current_sensor(sens))

    if CONF_ACTUAL_CHARGE_CURRENT in config:
        sens = yield sensor.new_sensor(config[CONF_ACTUAL_CHARGE_CURRENT])
        cg.add(var.set_actual_charge_current_sensor(sens))

    if CONF_VEHICLE_STATE in config:
        # TODO maybe adapt to sensor `new_sensor` method?
        local_config = config[CONF_VEHICLE_STATE]
        sens_var = cg.new_Pvariable(local_config[CONF_ID])
        yield text_sensor.register_text_sensor(sens_var, local_config)
        yield sens_var
        cg.add(var.set_vehicle_state_sensor(sens_var))

    if CONF_MAX_CURRENT_LIMIT in config:
        sens = yield sensor.new_sensor(config[CONF_MAX_CURRENT_LIMIT])
        cg.add(var.set_max_current_limit_sensor(sens))

    if CONF_FIRMWARE_REVISION in config:
        sens = yield sensor.new_sensor(config[CONF_FIRMWARE_REVISION])
        cg.add(var.set_firmware_revision_sensor(sens))

    if CONF_EVSE_STATE in config:
        local_config = config[CONF_EVSE_STATE]
        sens_var = cg.new_Pvariable(local_config[CONF_ID])
        yield text_sensor.register_text_sensor(sens_var, local_config)
        yield sens_var
        cg.add(var.set_evse_state_sensor(sens_var))

