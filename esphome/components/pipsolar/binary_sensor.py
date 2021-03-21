import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, binary_sensor
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_TEMPERATURE, \
    DEVICE_CLASS_TEMPERATURE,DEVICE_CLASS_POWER, ICON_EMPTY, UNIT_AMPERE, UNIT_CELSIUS, UNIT_HERTZ, UNIT_PERCENT, UNIT_VOLT, UNIT_EMPTY, UNIT_VOLT_AMPS, UNIT_WATT
from . import PipsolarComponent, pipsolar_ns

DEPENDENCIES = ['uart']

CONF_PIPSOLAR_ID = 'pipsolar_id'

#QPIGS
CONF_ADD_SBU_PRIORITY_VERSION = 'add_sbu_priority_version';
CONF_CONFIGURATION_STATUS = 'configuration_status';
CONF_SCC_FIRMWARE_VERSION = 'scc_firmware_version';
CONF_LOAD_STATUS = 'load_status';
CONF_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING = 'battery_voltage_to_steady_while_charging';
CONF_CHARGING_STATUS = 'charging_status';
CONF_SCC_CHARGING_STATUS = 'scc_charging_status';
CONF_AC_CHARGING_STATUS = 'ac_charging_status';
CONF_CHARGING_TO_FLOATING_MODE = 'charging_to_floating_mode';
CONF_SWITCH_ON = 'switch_on';
CONF_DUSTPROOF_INSTALLED = 'dustproof_installed';


pipsolar_binary_sensor_ns = cg.esphome_ns.namespace('pipsolarbinarysensor')

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(pipsolar_binary_sensor_ns),
    cv.Required(CONF_PIPSOLAR_ID): cv.use_id(PipsolarComponent),
    cv.Optional(CONF_ADD_SBU_PRIORITY_VERSION ): binary_sensor.BINARY_SENSOR_SCHEMA,
    cv.Optional(CONF_CONFIGURATION_STATUS ): binary_sensor.BINARY_SENSOR_SCHEMA,
    cv.Optional(CONF_SCC_FIRMWARE_VERSION ): binary_sensor.BINARY_SENSOR_SCHEMA,
    cv.Optional(CONF_LOAD_STATUS ): binary_sensor.BINARY_SENSOR_SCHEMA,
    cv.Optional(CONF_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING ): binary_sensor.BINARY_SENSOR_SCHEMA,
    cv.Optional(CONF_CHARGING_STATUS ): binary_sensor.BINARY_SENSOR_SCHEMA,
    cv.Optional(CONF_SCC_CHARGING_STATUS ): binary_sensor.BINARY_SENSOR_SCHEMA,
    cv.Optional(CONF_AC_CHARGING_STATUS ): binary_sensor.BINARY_SENSOR_SCHEMA,       
    cv.Optional(CONF_CHARGING_TO_FLOATING_MODE ): binary_sensor.BINARY_SENSOR_SCHEMA,
    cv.Optional(CONF_SWITCH_ON ): binary_sensor.BINARY_SENSOR_SCHEMA,
    cv.Optional(CONF_DUSTPROOF_INSTALLED ): binary_sensor.BINARY_SENSOR_SCHEMA,
        })



def to_code(config):
    paren = yield cg.get_variable(config[CONF_PIPSOLAR_ID])
    if CONF_ADD_SBU_PRIORITY_VERSION in config:
      conf = config[CONF_ADD_SBU_PRIORITY_VERSION]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_add_sbu_priority_version_sensor(sens))
    if CONF_CONFIGURATION_STATUS in config:
      conf = config[CONF_CONFIGURATION_STATUS]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_configuration_status_sensor(sens))
    if CONF_SCC_FIRMWARE_VERSION in config:
      conf = config[CONF_SCC_FIRMWARE_VERSION]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_scc_firmware_version_sensor(sens))
    if CONF_LOAD_STATUS in config:
      conf = config[CONF_LOAD_STATUS]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_load_status_sensor(sens))
    if CONF_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING in config:
      conf = config[CONF_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_battery_voltage_to_steady_while_charging_sensor(sens))
    if CONF_CHARGING_STATUS in config:
      conf = config[CONF_CHARGING_STATUS]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_charging_status_sensor(sens))
    if CONF_SCC_CHARGING_STATUS in config:
      conf = config[CONF_SCC_CHARGING_STATUS]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_scc_charging_status_sensor(sens))
    if CONF_AC_CHARGING_STATUS in config:
      conf = config[CONF_AC_CHARGING_STATUS]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_ac_charging_status_sensor(sens))
    if CONF_CHARGING_TO_FLOATING_MODE in config:
      conf = config[CONF_CHARGING_TO_FLOATING_MODE]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_charging_to_floating_mode_sensor(sens))
    if CONF_SWITCH_ON in config:
      conf = config[CONF_SWITCH_ON]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_switch_on_sensor(sens))
    if CONF_DUSTPROOF_INSTALLED in config:
      conf = config[CONF_DUSTPROOF_INSTALLED]
      sens = cg.new_Pvariable(conf[CONF_ID])
      yield binary_sensor.register_binary_sensor(sens, conf)
      cg.add(paren.set_dustproof_installed_sensor(sens))
