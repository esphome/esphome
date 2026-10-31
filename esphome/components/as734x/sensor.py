import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import UNIT_COUNTS
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR_TEMPERATURE,
    CONF_COUNTS,
    CONF_ILLUMINANCE,
    CONF_TYPE,
    DEVICE_CLASS_IRRADIANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_KELVIN,
    UNIT_PERCENT,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import BANDS_41, BANDS_43, CONF_AS734X_ID, DOMAIN, AS734XComponent

DEPENDENCIES = [DOMAIN]

CONF_BASIC_COUNTS = "basic_counts"
CONF_IRRADIANCE_PAR = "irradiance_par"
CONF_IRRADIANCE_PHOTOPIC = "irradiance_photopic"
CONF_PPFD = "ppfd"
CONF_SATURATION_LEVEL = "saturation_level"

UNIT_IRRADIANCE = "W/m\u00b2"
UNIT_PPFD = "\u00b5mol/m\u00b2/s"

ICON_COUNTS = "mdi:counter"
ICON_SATURATION = "mdi:weather-sunny-alert"
ICON_ILLUMINANCE = "mdi:weather-sunny"
ICON_IRRADIANCE_PHOTOPIC = "mdi:sun-wireless-outline"
ICON_PAR = "mdi:sprout"
ICON_PPFD = "mdi:sprout-outline"
ICON_COLOR_TEMPERATURE = "mdi:sun-thermometer-outline"

_COUNTS_SENSOR = sensor.sensor_schema(
    unit_of_measurement=UNIT_COUNTS,
    icon=ICON_COUNTS,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

# Basic counts are fractional, so they need decimals where raw counts do not.
_BASIC_COUNTS_SENSOR = sensor.sensor_schema(
    unit_of_measurement=UNIT_COUNTS,
    icon=ICON_COUNTS,
    accuracy_decimals=6,
    state_class=STATE_CLASS_MEASUREMENT,
)

_BAND_KEYS = (CONF_COUNTS, CONF_BASIC_COUNTS)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AS734X_ID): cv.use_id(AS734XComponent),
        cv.Optional(CONF_COUNTS, default={}): cv.Schema(
            {cv.Optional(band): _COUNTS_SENSOR for band in BANDS_43}
        ),
        cv.Optional(CONF_BASIC_COUNTS, default={}): cv.Schema(
            {cv.Optional(band): _BASIC_COUNTS_SENSOR for band in BANDS_43}
        ),
        cv.Optional(CONF_SATURATION_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon=ICON_SATURATION,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
            kind=sensor.ILLUMINANCE, icon=ICON_ILLUMINANCE
        ),
        cv.Optional(CONF_IRRADIANCE_PHOTOPIC): sensor.sensor_schema(
            unit_of_measurement=UNIT_IRRADIANCE,
            icon=ICON_IRRADIANCE_PHOTOPIC,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_IRRADIANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_IRRADIANCE_PAR): sensor.sensor_schema(
            unit_of_measurement=UNIT_IRRADIANCE,
            icon=ICON_PAR,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_IRRADIANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PPFD): sensor.sensor_schema(
            unit_of_measurement=UNIT_PPFD,
            icon=ICON_PPFD,
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_COLOR_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_KELVIN,
            icon=ICON_COLOR_TEMPERATURE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    hub = fv.get_declaring_config(config[CONF_AS734X_ID])
    if hub[CONF_TYPE] != "AS7341":
        return config
    for key in _BAND_KEYS:
        if extra := [band for band in config.get(key, {}) if band not in BANDS_41]:
            raise cv.Invalid(
                f"The AS7341 has no {', '.join(extra)} band", path=[key, extra[0]]
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AS734X_ID])
    counts = config[CONF_COUNTS]
    basic_counts = config[CONF_BASIC_COUNTS]
    for i, band in enumerate(BANDS_43):
        if (conf := counts.get(band)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_counts_sensor(sens, i))
        if (conf := basic_counts.get(band)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_basic_counts_sensor(sens, i))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_SATURATION_LEVEL, hub.set_saturation_level_sensor)
    await sensors(CONF_ILLUMINANCE, hub.set_illuminance_sensor)
    await sensors(CONF_IRRADIANCE_PHOTOPIC, hub.set_irradiance_photopic_sensor)
    await sensors(CONF_IRRADIANCE_PAR, hub.set_irradiance_par_sensor)
    await sensors(CONF_PPFD, hub.set_ppfd_sensor)
    await sensors(CONF_COLOR_TEMPERATURE, hub.set_color_temperature_sensor)
