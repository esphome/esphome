import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_IAQ_ACCURACY,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    ICON_GAS_CYLINDER,
    ICON_GAUGE,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_OHM,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

from . import CONF_BME690_ID, BME690Component

DEPENDENCIES = ["bme690"]

CONF_GAS_RESISTANCE = "gas_resistance"
CONF_IAQ = "iaq"
CONF_STATIC_IAQ = "static_iaq"
CONF_CO2_EQUIVALENT = "co2_equivalent"
CONF_BREATH_VOC_EQUIVALENT = "breath_voc_equivalent"
CONF_GAS_PERCENTAGE = "gas_percentage"
CONF_COMPENSATED_TEMPERATURE = "compensated_temperature"
CONF_COMPENSATED_HUMIDITY = "compensated_humidity"

UNIT_IAQ = "IAQ"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BME690_ID): cv.use_id(BME690Component),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_THERMOMETER,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon=ICON_WATER_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_HECTOPASCAL,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_GAS_RESISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_OHM,
            icon=ICON_GAS_CYLINDER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_IAQ): sensor.sensor_schema(
            unit_of_measurement=UNIT_IAQ,
            icon=ICON_GAUGE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_IAQ_ACCURACY): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_STATIC_IAQ): sensor.sensor_schema(
            unit_of_measurement=UNIT_IAQ,
            icon=ICON_GAUGE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CO2_EQUIVALENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BREATH_VOC_EQUIVALENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_GAS_PERCENTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_COMPENSATED_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_THERMOMETER,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_COMPENSATED_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon=ICON_WATER_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BME690_ID])

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(hub.set_temperature_sensor(sens))
    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(hub.set_humidity_sensor(sens))
    if CONF_PRESSURE in config:
        sens = await sensor.new_sensor(config[CONF_PRESSURE])
        cg.add(hub.set_pressure_sensor(sens))
    if CONF_GAS_RESISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_GAS_RESISTANCE])
        cg.add(hub.set_gas_resistance_sensor(sens))
    if CONF_IAQ in config:
        sens = await sensor.new_sensor(config[CONF_IAQ])
        cg.add(hub.set_iaq_sensor(sens))
    if CONF_IAQ_ACCURACY in config:
        sens = await sensor.new_sensor(config[CONF_IAQ_ACCURACY])
        cg.add(hub.set_iaq_accuracy_sensor(sens))
    if CONF_STATIC_IAQ in config:
        sens = await sensor.new_sensor(config[CONF_STATIC_IAQ])
        cg.add(hub.set_static_iaq_sensor(sens))
    if CONF_CO2_EQUIVALENT in config:
        sens = await sensor.new_sensor(config[CONF_CO2_EQUIVALENT])
        cg.add(hub.set_co2_equivalent_sensor(sens))
    if CONF_BREATH_VOC_EQUIVALENT in config:
        sens = await sensor.new_sensor(config[CONF_BREATH_VOC_EQUIVALENT])
        cg.add(hub.set_breath_voc_equivalent_sensor(sens))
    if CONF_GAS_PERCENTAGE in config:
        sens = await sensor.new_sensor(config[CONF_GAS_PERCENTAGE])
        cg.add(hub.set_gas_percentage_sensor(sens))
    if CONF_COMPENSATED_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_COMPENSATED_TEMPERATURE])
        cg.add(hub.set_comp_temperature_sensor(sens))
    if CONF_COMPENSATED_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_COMPENSATED_HUMIDITY])
        cg.add(hub.set_comp_humidity_sensor(sens))
