import esphome.codegen as cg
from esphome.components import modbus, sensor
from esphome.components.atm90e32.sensor import CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C
import esphome.config_validation as cv
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
    CONF_TOTAL_POWER,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    ICON_FLASH,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_DEGREES,
    UNIT_HERTZ,
    UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
)

AUTO_LOAD = ["modbus"]
CODEOWNERS = ["@polyfaces", "@jesserockz"]

sdm_meter_ns = cg.esphome_ns.namespace("sdm_meter")
SDMMeter = sdm_meter_ns.class_("SDMMeter", cg.PollingComponent, modbus.ModbusDevice)

PHASE_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_APPARENT_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_REACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_POWER_FACTOR: sensor.sensor_schema(
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER_FACTOR,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_PHASE_ANGLE: sensor.sensor_schema(
        unit_of_measurement=UNIT_DEGREES, icon=ICON_FLASH, accuracy_decimals=3
    ),
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
                unit_of_measurement=UNIT_HERTZ,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TOTAL_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_IMPORT_ACTIVE_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_EXPORT_ACTIVE_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_IMPORT_REACTIVE_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
                accuracy_decimals=2,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_EXPORT_REACTIVE_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
                accuracy_decimals=2,
                state_class=STATE_CLASS_TOTAL_INCREASING,
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

    if CONF_TOTAL_POWER in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_POWER])
        cg.add(var.set_total_power_sensor(sens))

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
