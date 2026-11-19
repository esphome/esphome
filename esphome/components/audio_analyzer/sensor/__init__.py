import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, STATE_CLASS_MEASUREMENT, UNIT_DECIBEL
from esphome.types import ConfigType

from .. import CONF_AUDIO_ANALYZER_ID, NUM_BANDS, AudioAnalyzer, audio_analyzer_ns

DEPENDENCIES = ["audio_analyzer"]

CONF_BAND = "band"
CONF_BANDS = "bands"
CONF_RMS = "rms"

AudioAnalyzerSensor = audio_analyzer_ns.class_(
    "AudioAnalyzerSensor", cg.PollingComponent
)


def _validate_unique_bands(config: ConfigType) -> ConfigType:
    bands = [band_config[CONF_BAND] for band_config in config.get(CONF_BANDS, [])]
    if len(bands) != len(set(bands)):
        raise cv.Invalid("Each band can only have one sensor")
    return config


def _db_sensor_schema(icon: str) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_DECIBEL,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        icon=icon,
    )


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AudioAnalyzerSensor),
            cv.GenerateID(CONF_AUDIO_ANALYZER_ID): cv.use_id(AudioAnalyzer),
            cv.Optional(CONF_RMS): _db_sensor_schema("mdi:waveform"),
            cv.Optional(CONF_BANDS): cv.All(
                cv.ensure_list(
                    _db_sensor_schema("mdi:equalizer").extend(
                        {cv.Required(CONF_BAND): cv.int_range(min=0, max=NUM_BANDS - 1)}
                    )
                ),
                cv.Length(min=1),
            ),
        }
    ).extend(cv.polling_component_schema("1s")),
    cv.has_at_least_one_key(CONF_RMS, CONF_BANDS),
    _validate_unique_bands,
)


async def to_code(config: ConfigType) -> None:
    analyzer = await cg.get_variable(config[CONF_AUDIO_ANALYZER_ID])
    var = cg.new_Pvariable(config[CONF_ID], analyzer)
    await cg.register_component(var, config)
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_RMS, var.set_rms_sensor)
    for band_config in config.get(CONF_BANDS, []):
        sens = await sensor.new_sensor(band_config)
        cg.add(var.set_band_sensor(band_config[CONF_BAND], sens))
