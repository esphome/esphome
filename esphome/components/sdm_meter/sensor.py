from esphome.components.atm90e32.sensor import CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, modbus
from esphome.const import (
    CONF_ACTIVE_POWER,
    CONF_APPARENT_POWER,
    CONF_CURRENT,
    CONF_EXPORT_ACTIVE_ENERGY,
    CONF_EXPORT_REACTIVE_ENERGY,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_IMPORT_ACTIVE_ENERGY,
    CONF_IMPORT_REACTIVE_ENERGY,
    CONF_PHASE_ANGLE,
    CONF_POWER_FACTOR,
    CONF_REACTIVE_POWER,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    ICON_EMPTY,
    ICON_FLASH,
    LAST_RESET_TYPE_AUTO,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_DEGREES,
    UNIT_EMPTY,
    UNIT_HERTZ,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_VOLT_AMPS_REACTIVE_HOURS,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

AUTO_LOAD = ["modbus"]
CODEOWNERS = ["@polyfaces", "@jesserockz"]

sdm_meter_ns = cg.esphome_ns.namespace("sdm_meter")
SDMMeter = sdm_meter_ns.class_("SDMMeter", cg.PollingComponent, modbus.ModbusDevice)

PHASE_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(UNIT_VOLT, ICON_EMPTY, 2, DEVICE_CLASS_VOLTAGE),
    CONF_CURRENT: sensor.sensor_schema(
        UNIT_AMPERE, ICON_EMPTY, 3, DEVICE_CLASS_CURRENT, STATE_CLASS_MEASUREMENT
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        UNIT_WATT, ICON_EMPTY, 2, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
    ),
    CONF_APPARENT_POWER: sensor.sensor_schema(
        UNIT_VOLT_AMPS, ICON_EMPTY, 2, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
    ),
    CONF_REACTIVE_POWER: sensor.sensor_schema(
        UNIT_VOLT_AMPS_REACTIVE,
        ICON_EMPTY,
        2,
        DEVICE_CLASS_POWER,
        STATE_CLASS_MEASUREMENT,
    ),
    CONF_POWER_FACTOR: sensor.sensor_schema(
        UNIT_EMPTY, ICON_EMPTY, 3, DEVICE_CLASS_POWER_FACTOR, STATE_CLASS_MEASUREMENT
    ),
    CONF_PHASE_ANGLE: sensor.sensor_schema(UNIT_DEGREES, ICON_FLASH, 3),
}

PHASE_SCHEMA = cv.Schema(
    {cv.Optional(sensor): schema for sensor, schema in PHASE_SENSORS.items()}
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SDMMeter),
            cv.Optional(CONF_PHASE_A): PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_B): PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_C): PHASE_SCHEMA,
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                UNIT_HERTZ,
                ICON_CURRENT_AC,
                3,
                DEVICE_CLASS_EMPTY,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_IMPORT_ACTIVE_ENERGY): sensor.sensor_schema(
                UNIT_WATT_HOURS,
                ICON_EMPTY,
                2,
                DEVICE_CLASS_ENERGY,
                STATE_CLASS_MEASUREMENT,
                LAST_RESET_TYPE_AUTO,
            ),
            cv.Optional(CONF_EXPORT_ACTIVE_ENERGY): sensor.sensor_schema(
                UNIT_WATT_HOURS,
                ICON_EMPTY,
                2,
                DEVICE_CLASS_ENERGY,
                STATE_CLASS_MEASUREMENT,
                LAST_RESET_TYPE_AUTO,
            ),
            cv.Optional(CONF_IMPORT_REACTIVE_ENERGY): sensor.sensor_schema(
                UNIT_VOLT_AMPS_REACTIVE_HOURS,
                ICON_EMPTY,
                2,
                DEVICE_CLASS_ENERGY,
                STATE_CLASS_MEASUREMENT,
                LAST_RESET_TYPE_AUTO,
            ),
            cv.Optional(CONF_EXPORT_REACTIVE_ENERGY): sensor.sensor_schema(
                UNIT_VOLT_AMPS_REACTIVE_HOURS,
                ICON_EMPTY,
                2,
                DEVICE_CLASS_ENERGY,
                STATE_CLASS_MEASUREMENT,
                LAST_RESET_TYPE_AUTO,
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(modbus.modbus_device_schema(0x01))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_device(var, config)

    if CONF_FREQUENCY in config:
        sens = await sensor.new_sensor(config[CONF_FREQUENCY])
        cg.add(var.set_frequency_sensor(sens))

    if CONF_IMPORT_ACTIVE_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_IMPORT_ACTIVE_ENERGY])
        cg.add(var.set_import_active_energy_sensor(sens))

    if CONF_EXPORT_ACTIVE_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_EXPORT_ACTIVE_ENERGY])
        cg.add(var.set_export_active_energy_sensor(sens))

    if CONF_IMPORT_REACTIVE_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_IMPORT_REACTIVE_ENERGY])
        cg.add(var.set_import_reactive_energy_sensor(sens))

    if CONF_EXPORT_REACTIVE_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_EXPORT_REACTIVE_ENERGY])
        cg.add(var.set_export_reactive_energy_sensor(sens))

    for i, phase in enumerate([CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C]):
        if phase not in config:
            continue

        phase_config = config[phase]
        for sensor_type in PHASE_SENSORS:
            if sensor_type in phase_config:
                sens = await sensor.new_sensor(phase_config[sensor_type])
                cg.add(getattr(var, f"set_{sensor_type}_sensor")(i, sens))
