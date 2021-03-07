from esphome.components import climate, sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, UNIT_CELSIUS, UNIT_PERCENT, ICON_THERMOMETER, DEVICE_CLASS_TEMPERATURE, ICON_WATER_PERCENT, DEVICE_CLASS_HUMIDITY
from esphome.components.midea_dongle import CONF_MIDEA_DONGLE_ID, MideaDongle

DEPENDENCIES = ['midea_dongle']
AUTO_LOAD = ['climate', 'sensor']
CODEOWNERS = ['@dudanov']

CONF_BEEPER = 'beeper'
CONF_SWING_HORIZONTAL = 'swing_horizontal'
CONF_SWING_BOTH = 'swing_both'
CONF_OUTDOOR_TEMPERATURE = 'outdoor_temperature'
CONF_HUMIDITY_SETPOINT = 'humidity_setpoint'
midea_ac_ns = cg.esphome_ns.namespace('midea_ac')
MideaAC = midea_ac_ns.class_('MideaAC', climate.Climate, cg.Component)

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(MideaAC),
    cv.GenerateID(CONF_MIDEA_DONGLE_ID): cv.use_id(MideaDongle),
    cv.Optional(CONF_BEEPER): cv.boolean,
    cv.Optional(CONF_SWING_HORIZONTAL): cv.boolean,
    cv.Optional(CONF_SWING_BOTH): cv.boolean,
    cv.Optional(CONF_OUTDOOR_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 0, DEVICE_CLASS_TEMPERATURE),
    cv.Optional(CONF_HUMIDITY_SETPOINT): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 0, DEVICE_CLASS_HUMIDITY),
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)
    paren = yield cg.get_variable(config[CONF_MIDEA_DONGLE_ID])
    cg.add(var.set_midea_dongle_parent(paren))
    if CONF_BEEPER in config:
        cg.add(var.set_beeper_feedback(config[CONF_BEEPER]))
    if CONF_SWING_HORIZONTAL in config:
        cg.add(var.set_swing_horizontal(config[CONF_SWING_HORIZONTAL]))
    if CONF_SWING_BOTH in config:
        cg.add(var.set_swing_both(config[CONF_SWING_BOTH]))
    if CONF_OUTDOOR_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_OUTDOOR_TEMPERATURE])
        cg.add(var.set_outdoor_temperature_sensor(sens))
    if CONF_HUMIDITY_SETPOINT in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY_SETPOINT])
        cg.add(var.set_humidity_setpoint_sensor(sens))
