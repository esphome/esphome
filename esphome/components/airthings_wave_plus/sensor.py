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
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await airthings_wave_base.wave_base_to_code(var, config)

    if CONF_RADON in config:
        sens = await sensor.new_sensor(config[CONF_RADON])
        cg.add(var.set_radon(sens))
    if CONF_RADON_LONG_TERM in config:
        sens = await sensor.new_sensor(config[CONF_RADON_LONG_TERM])
        cg.add(var.set_radon_long_term(sens))
    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2(sens))
