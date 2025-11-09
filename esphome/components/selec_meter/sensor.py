import esphome.codegen as cg
from esphome.components import modbus, sensor
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
    CONF_POWER_FACTOR,
    CONF_REACTIVE_POWER,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_HERTZ,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
)

AUTO_LOAD = ["modbus"]
CODEOWNERS = ["@sourabhjaiswal"]

CONF_TOTAL_ACTIVE_ENERGY = "total_active_energy"
CONF_TOTAL_REACTIVE_ENERGY = "total_reactive_energy"
CONF_APPARENT_ENERGY = "apparent_energy"
CONF_MAXIMUM_DEMAND_ACTIVE_POWER = "maximum_demand_active_power"
CONF_MAXIMUM_DEMAND_REACTIVE_POWER = "maximum_demand_reactive_power"
CONF_MAXIMUM_DEMAND_APPARENT_POWER = "maximum_demand_apparent_power"

UNIT_KILOWATT_HOURS = "kWh"
UNIT_KILOVOLT_AMPS_HOURS = "kVAh"
UNIT_KILOVOLT_AMPS_REACTIVE_HOURS = "kVARh"

selec_meter_ns = cg.esphome_ns.namespace("selec_meter")
SelecMeter = selec_meter_ns.class_(
    "SelecMeter", cg.PollingComponent, modbus.ModbusDevice
)

SENSORS = {
    CONF_TOTAL_ACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_IMPORT_ACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_EXPORT_ACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_TOTAL_REACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_IMPORT_REACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_EXPORT_REACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_APPARENT_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_HOURS,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_REACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_APPARENT_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
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
    CONF_POWER_FACTOR: sensor.sensor_schema(
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER_FACTOR,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_FREQUENCY: sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_CURRENT_AC,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_MAXIMUM_DEMAND_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_MAXIMUM_DEMAND_REACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_MAXIMUM_DEMAND_APPARENT_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(SelecMeter)})
    .extend(
        {cv.Optional(sensor_name): schema for sensor_name, schema in SENSORS.items()}
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(modbus.modbus_device_schema(0x01))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_device(var, config)
    for name in SENSORS:
        if name in config:
            sens = await sensor.new_sensor(config[name])
            cg.add(getattr(var, f"set_{name}_sensor")(sens))
