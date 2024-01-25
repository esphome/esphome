import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_BUS_VOLTAGE,
    CONF_CURRENT,
    CONF_ENERGY,
    CONF_MAX_CURRENT,
    CONF_POWER,
    CONF_SHUNT_RESISTANCE,
    CONF_SHUNT_VOLTAGE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

CODEOWNERS = ["@latonita"]

CONF_ADC_RANGE = "adc_range"
CONF_CHARGE = "charge"
CONF_CHARGE_COULOMB = "charge_coulombs"
CONF_ENERGY_JOULE = "energy_joules"
CONF_TEMPERATURE_COEFFICIENT = "temperature_coefficient"
UNIT_AMPERE_HOURS = "Ah"
UNIT_JOULE = "J"
UNIT_COULOMB = "C"
UNIT_MILLIVOLT = "mV"

ina2xx_base_ns = cg.esphome_ns.namespace("ina2xx_base")
INA2XX = ina2xx_base_ns.class_("INA2XX", cg.PollingComponent)

INA2XX_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SHUNT_RESISTANCE): cv.All(cv.resistance, cv.Range(min=0.0)),
        cv.Required(CONF_MAX_CURRENT): cv.All(cv.current, cv.Range(min=0.0)),
        cv.Optional(CONF_ADC_RANGE, default=0): cv.int_range(min=0, max=1),
        cv.Optional(CONF_TEMPERATURE_COEFFICIENT, default=0): cv.int_range(
            min=0, max=16383
        ),
        cv.Optional(CONF_SHUNT_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIVOLT,
            accuracy_decimals=5,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BUS_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=5,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=5,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=8,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=6,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT_HOURS,
            accuracy_decimals=8,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ENERGY_JOULE): sensor.sensor_schema(
            unit_of_measurement=UNIT_JOULE,
            accuracy_decimals=8,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CHARGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE_HOURS,
            accuracy_decimals=8,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CHARGE_COULOMB): sensor.sensor_schema(
            unit_of_measurement=UNIT_COULOMB,
            accuracy_decimals=8,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def setup_ina2xx(var, config):
    await cg.register_component(var, config)

    cg.add(var.set_shunt_resistance_ohm(config[CONF_SHUNT_RESISTANCE]))
    cg.add(var.set_max_current_a(config[CONF_MAX_CURRENT]))
    cg.add(var.set_adc_range(config[CONF_ADC_RANGE]))
    cg.add(var.set_shunt_tempco(config[CONF_TEMPERATURE_COEFFICIENT]))

    if conf := config.get(CONF_SHUNT_VOLTAGE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_shunt_voltage_sensor(sens))

    if conf := config.get(CONF_BUS_VOLTAGE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bus_voltage_sensor(sens))

    if conf := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_die_temperature_sensor(sens))

    if conf := config.get(CONF_CURRENT):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_current_sensor(sens))

    if conf := config.get(CONF_POWER):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power_sensor(sens))

    if conf := config.get(CONF_ENERGY):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_sensor_wh(sens))

    if conf := config.get(CONF_ENERGY_JOULE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_sensor_j(sens))

    if conf := config.get(CONF_CHARGE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_sensor_ah(sens))

    if conf := config.get(CONF_CHARGE_COULOMB):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_sensor_c(sens))
