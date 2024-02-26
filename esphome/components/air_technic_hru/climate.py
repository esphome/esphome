import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import modbus, sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_TEMPERATURE,
    UNIT_CELSIUS,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    CONF_SENSORS
)

AUTO_LOAD = ["sensor", "modbus"]

CONF_FAN_SPEED = "fan_speed"

air_technic_hru_ns = cg.esphome_ns.namespace("air_techic_hru")
AirTechnicHru = air_technic_hru_ns.class_("AirTechnicHru", sensor.Sensor, cg.PollingComponent, modbus.ModbusDevice)

CONFIG_SCHEMA = (
    cv.Optional(CONF_SENSORS,
                sensor.sensor_schema(AirTechnicHru, accuracy_decimals=1))
    .extend(cv.polling_component_schema("10s"))
    .extend(modbus.modbus_device_schema(0x01))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_device(var, config)

    if CONF_SENSORS in config:
        conf = config[CONF_SENSORS]
        sens = await sensor.new_sensor(conf)
        cg.register_component(sens, conf)
