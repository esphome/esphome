import esphome.codegen as cg
from esphome.components import airthings_wave_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_RADON,
    CONF_RADON_LONG_TERM,
    CONF_TVOC,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_RADIOACTIVE,
    STATE_CLASS_MEASUREMENT,
    UNIT_BECQUEREL_PER_CUBIC_METER,
    UNIT_LUX,
    UNIT_PARTS_PER_MILLION,
)
from esphome.types import ConfigType

DEPENDENCIES = airthings_wave_base.DEPENDENCIES

AUTO_LOAD = ["airthings_wave_base"]

airthings_wave_plus_ns = cg.esphome_ns.namespace("airthings_wave_plus")
AirthingsWavePlus = airthings_wave_plus_ns.class_(
    "AirthingsWavePlus", airthings_wave_base.AirthingsWaveBase
)

CONF_DEVICE_TYPE = "device_type"
WaveDeviceType = airthings_wave_plus_ns.enum("WaveDeviceType")
DEVICE_TYPES = {
    "WAVE_PLUS": WaveDeviceType.WAVE_PLUS,
    "WAVE_GEN2": WaveDeviceType.WAVE_GEN2,
}


def validate_wave_gen2_config(config: ConfigType) -> ConfigType:
    """Validate that Wave Gen2 devices don't have CO2 or TVOC sensors."""
    if config[CONF_DEVICE_TYPE] == "WAVE_GEN2":
        if CONF_CO2 in config:
            raise cv.Invalid("Wave Gen2 devices do not support CO2 sensor")
        # Check for TVOC in the base schema config
        if CONF_TVOC in config:
            raise cv.Invalid("Wave Gen2 devices do not support TVOC sensor")
    return config


CONFIG_SCHEMA = cv.All(
    airthings_wave_base.BASE_SCHEMA.extend(
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
            cv.Optional(CONF_DEVICE_TYPE, default="WAVE_PLUS"): cv.enum(
                DEVICE_TYPES, upper=True
            ),
        }
    ),
    validate_wave_gen2_config,
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
    cg.add(var.set_device_type(config[CONF_DEVICE_TYPE]))
