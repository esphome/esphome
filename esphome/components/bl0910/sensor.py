import esphome.codegen as cg
from esphome.components import sensor, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_DEGREES,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

# 10 sensors in range 1-11
SENSOR_RANGE = range(1, 12)

DEPENDENCIES = ["spi"]

bl0910_ns = cg.esphome_ns.namespace("bl0910")
BL0910 = bl0910_ns.class_("BL0910", cg.PollingComponent, spi.SPIDevice)

VOLTAGE_CB = {
    cv.Optional(f"voltage_{i}"): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend({cv.Optional("uref", default=1.0): cv.float_})
    for i in SENSOR_RANGE
}

CURRENT_CV = {
    cv.Optional(f"current_{i}"): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend({cv.Optional("iref", default=1.0): cv.float_})
    for i in SENSOR_RANGE
}

ACTIVE_POWER_CV = {
    cv.Optional(f"active_power_{i}"): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend({cv.Optional("pref", default=1.0): cv.float_})
    for i in SENSOR_RANGE
}

ENERGY_CV = {
    cv.Optional(f"energy_{i}"): sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ).extend({cv.Optional("eref", default=1.0): cv.float_})
    for i in SENSOR_RANGE
}

POWER_FACTOR_CV = {
    cv.Optional(f"power_factor_{i}"): sensor.sensor_schema(
        unit_of_measurement=UNIT_DEGREES,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER_FACTOR,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    for i in SENSOR_RANGE
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BL0910),
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                unit_of_measurement="Hz",
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_FREQUENCY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema())
    .extend(VOLTAGE_CB)
    .extend(CURRENT_CV)
    .extend(ACTIVE_POWER_CV)
    .extend(ENERGY_CV)
    .extend(POWER_FACTOR_CV)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if frequency_config := config.get(CONF_FREQUENCY):
        sens = await sensor.new_sensor(frequency_config)
        cg.add(var.set_frequency_sensor(sens))
    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))

    for i in SENSOR_RANGE:
        if voltage_config := config.get(f"voltage_{i}"):
            sens = await sensor.new_sensor(voltage_config)
            cg.add(var.set_voltage_sensor(sens, i, voltage_config.get("uref")))
        if current_config := config.get(f"current_{i}"):
            sens = await sensor.new_sensor(current_config)
            cg.add(var.set_current_sensor(sens, i, current_config.get("iref")))
        if active_power_config := config.get(f"active_power_{i}"):
            sens = await sensor.new_sensor(active_power_config)
            cg.add(var.set_power_sensor(sens, i, active_power_config.get("pref")))
        if energy_config := config.get(f"energy_{i}"):
            sens = await sensor.new_sensor(energy_config)
            cg.add(var.set_energy_sensor(sens, i, energy_config.get("eref")))
        if power_factor_config := config.get(f"power_factor_{i}"):
            sens = await sensor.new_sensor(power_factor_config)
            cg.add(var.set_power_factor_sensor(sens, i))
