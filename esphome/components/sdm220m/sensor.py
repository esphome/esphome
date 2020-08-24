import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, modbus
from esphome.const import CONF_ACTIVE_POWER, CONF_APPARENT_POWER, \
    CONF_CURRENT, CONF_EXPORT_ACTIVE_ENERGY, \
    CONF_EXPORT_REACTIVE_ENERGY, CONF_FREQUENCY, CONF_ID, \
    CONF_IMPORT_ACTIVE_ENERGY, CONF_IMPORT_REACTIVE_ENERGY, \
    CONF_PHASE_ANGLE, CONF_POWER_FACTOR, CONF_REACTIVE_POWER, \
    CONF_VOLTAGE, ICON_COUNTER, ICON_CURRENT_AC, ICON_FLASH, \
    ICON_POWER, UNIT_AMPERE, UNIT_DEGREES, UNIT_EMPTY, UNIT_HERTZ, \
    UNIT_VOLT, UNIT_VOLT_AMPS, UNIT_VOLT_AMPS_REACTIVE, \
    UNIT_VOLT_AMPS_REACTIVE_HOURS, UNIT_WATT, UNIT_WATT_HOURS

AUTO_LOAD = ['modbus']

sdm220m_ns = cg.esphome_ns.namespace('sdm220m')
SDM220M = sdm220m_ns.class_('SDM220M', cg.PollingComponent, modbus.ModbusDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SDM220M),
    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 2),
    cv.Optional(CONF_CURRENT): sensor.sensor_schema(UNIT_AMPERE, ICON_CURRENT_AC, 3),
    cv.Optional(CONF_ACTIVE_POWER): sensor.sensor_schema(UNIT_WATT, ICON_POWER, 2),
    cv.Optional(CONF_APPARENT_POWER): sensor.sensor_schema(UNIT_VOLT_AMPS, ICON_POWER, 2),
    cv.Optional(CONF_REACTIVE_POWER): sensor.sensor_schema(UNIT_VOLT_AMPS_REACTIVE, ICON_POWER, 2),
    cv.Optional(CONF_POWER_FACTOR): sensor.sensor_schema(UNIT_EMPTY, ICON_FLASH, 3),
    cv.Optional(CONF_PHASE_ANGLE): sensor.sensor_schema(UNIT_DEGREES, ICON_FLASH, 3),
    cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(UNIT_HERTZ, ICON_CURRENT_AC, 3),
    cv.Optional(CONF_IMPORT_ACTIVE_ENERGY): sensor.sensor_schema(UNIT_WATT_HOURS, ICON_COUNTER, 2),
    cv.Optional(CONF_EXPORT_ACTIVE_ENERGY): sensor.sensor_schema(UNIT_WATT_HOURS, ICON_COUNTER, 2),
    cv.Optional(CONF_IMPORT_REACTIVE_ENERGY): sensor.sensor_schema(UNIT_VOLT_AMPS_REACTIVE_HOURS,
                                                                   ICON_COUNTER, 2),
    cv.Optional(CONF_EXPORT_REACTIVE_ENERGY): sensor.sensor_schema(UNIT_VOLT_AMPS_REACTIVE_HOURS,
                                                                   ICON_COUNTER, 2),
}).extend(cv.polling_component_schema('10s')).extend(modbus.modbus_device_schema(0x01))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield modbus.register_modbus_device(var, config)

    if CONF_VOLTAGE in config:
        conf = config[CONF_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_voltage_sensor(sens))
    if CONF_CURRENT in config:
        conf = config[CONF_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_current_sensor(sens))
    if CONF_ACTIVE_POWER in config:
        conf = config[CONF_ACTIVE_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_active_power_sensor(sens))
    if CONF_APPARENT_POWER in config:
        conf = config[CONF_APPARENT_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_apparent_power_sensor(sens))
    if CONF_REACTIVE_POWER in config:
        conf = config[CONF_REACTIVE_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_reactive_power_sensor(sens))
    if CONF_POWER_FACTOR in config:
        conf = config[CONF_POWER_FACTOR]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_power_factor_sensor(sens))
    if CONF_PHASE_ANGLE in config:
        conf = config[CONF_PHASE_ANGLE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_phase_angle_sensor(sens))
    if CONF_FREQUENCY in config:
        conf = config[CONF_FREQUENCY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_frequency_sensor(sens))
    if CONF_IMPORT_ACTIVE_ENERGY in config:
        conf = config[CONF_IMPORT_ACTIVE_ENERGY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_import_active_energy_sensor(sens))
    if CONF_EXPORT_ACTIVE_ENERGY in config:
        conf = config[CONF_EXPORT_ACTIVE_ENERGY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_export_active_energy_sensor(sens))
    if CONF_IMPORT_REACTIVE_ENERGY in config:
        conf = config[CONF_IMPORT_REACTIVE_ENERGY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_import_reactive_energy_sensor(sens))
    if CONF_EXPORT_REACTIVE_ENERGY in config:
        conf = config[CONF_EXPORT_REACTIVE_ENERGY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_export_reactive_energy_sensor(sens))
