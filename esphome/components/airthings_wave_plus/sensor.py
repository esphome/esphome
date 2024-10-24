import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, airthings_wave_base

from esphome.const import (
    DEVICE_CLASS_CARBON_DIOXIDE,
    STATE_CLASS_MEASUREMENT,
    ICON_RADIOACTIVE,
    CONF_ID,
    CONF_RADON,
    CONF_RADON_LONG_TERM,
    CONF_CO2,
    UNIT_BECQUEREL_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION,
    CONF_ILLUMINANCE,
    UNIT_LUX,
    DEVICE_CLASS_ILLUMINANCE,
)

DEPENDENCIES = airthings_wave_base.DEPENDENCIES

AUTO_LOAD = ["airthings_wave_base"]

airthings_wave_plus_ns = cg.esphome_ns.namespace("airthings_wave_plus")
AirthingsWavePlus = airthings_wave_plus_ns.class_(
    "AirthingsWavePlus", airthings_wave_base.AirthingsWaveBase
)


CONFIG_SCHEMA = airthings_wave_base.BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AirthingsWavePlus),
        cv.Optional(CONF_RADON): sensor.sensor_schema(
            unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
            icon=ICON_RADIOACTIVE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RADON_LONG_TERM): sensor.sensor_schema(
            unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
            icon=ICON_RADIOACTIVE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CO2): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_LUX,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await airthings_wave_base.wave_base_to_code(var, config)

    if config_radon := config.get(CONF_RADON):
        sens = await sensor.new_sensor(config_radon)
        cg.add(var.set_radon(sens))
    if config_radon_long_term := config.get(CONF_RADON_LONG_TERM):
        sens = await sensor.new_sensor(config_radon_long_term)
        cg.add(var.set_radon_long_term(sens))
    if config_co2 := config.get(CONF_CO2):
        sens = await sensor.new_sensor(config_co2)
        cg.add(var.set_co2(sens))
    if config_illuminance := config.get(CONF_ILLUMINANCE):
        sens = await sensor.new_sensor(config_illuminance)
        cg.add(var.set_illuminance(sens))
