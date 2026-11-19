import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_POWER, STATE_CLASS_MEASUREMENT, UNIT_DECIBEL
from esphome.types import ConfigType

from . import CONF_AUDIO_ANALYZER_ID, NUM_BANDS, AudioAnalyzer

DEPENDENCIES = ["audio_analyzer"]

CONF_BAND = "band"
CONF_BANDS = "bands"


def _validate_unique_bands(config: ConfigType) -> ConfigType:
    bands = [band_config[CONF_BAND] for band_config in config[CONF_BANDS]]
    if len(bands) != len(set(bands)):
        raise cv.Invalid("Each band can only have one sensor")
    return config


# Sensors publish the highest value seen since the previous update, so short sounds stay visible at a low rate.
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_AUDIO_ANALYZER_ID): cv.use_id(AudioAnalyzer),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                icon="mdi:waveform",
            ),
            cv.Optional(CONF_BANDS, default=[]): cv.ensure_list(
                sensor.sensor_schema(
                    accuracy_decimals=2,
                    state_class=STATE_CLASS_MEASUREMENT,
                    icon="mdi:equalizer",
                ).extend(
                    {cv.Required(CONF_BAND): cv.int_range(min=0, max=NUM_BANDS - 1)}
                )
            ),
        }
    ),
    _validate_unique_bands,
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AUDIO_ANALYZER_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_POWER, hub.set_power_sensor)
    for band_config in config[CONF_BANDS]:
        sens = await sensor.new_sensor(band_config)
        cg.add(hub.set_band_sensor(band_config[CONF_BAND], sens))
