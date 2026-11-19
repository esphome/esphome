"""Live power and frequency spectrum of an audio stream.

One analysis engine with two inputs:
- ``audio_analyzer:`` analyzes a microphone, hooked up like ``sound_level``.
- ``speaker: - platform: audio_analyzer`` is a pass-through speaker that analyzes the audio it plays.

Values are read in lambdas through the getters; ``sensor: - platform: audio_analyzer`` publishes them at a low rate.
"""

import esphome.codegen as cg
from esphome.components import microphone
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MICROPHONE, PLATFORM_ESP32
from esphome.types import ConfigType

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@jlpouffier"]
DOMAIN = "audio_analyzer"
MULTI_CONF = True

CONF_ATTACK = "attack"
CONF_AUDIO_ANALYZER_ID = "audio_analyzer_id"
CONF_BAND_RANGE = "band_range"
CONF_MAX_DB = "max_db"
CONF_MAX_FREQUENCY = "max_frequency"
CONF_MIN_DB = "min_db"
CONF_MIN_FREQUENCY = "min_frequency"
CONF_PASSIVE = "passive"
CONF_POWER_RANGE = "power_range"
CONF_RELEASE = "release"

NUM_BANDS = 12

audio_analyzer_ns = cg.esphome_ns.namespace("audio_analyzer")
AudioAnalyzer = audio_analyzer_ns.class_("AudioAnalyzer", cg.PollingComponent)
MicrophoneAudioAnalyzer = audio_analyzer_ns.class_(
    "MicrophoneAudioAnalyzer", AudioAnalyzer
)


def _validate_db_range(config: ConfigType) -> ConfigType:
    if config[CONF_MIN_DB] >= config[CONF_MAX_DB]:
        raise cv.Invalid(f"{CONF_MIN_DB} must be below {CONF_MAX_DB}")
    return config


def _db_range_schema(min_default: float, max_default: float) -> cv.All:
    return cv.All(
        cv.Schema(
            {
                cv.Optional(CONF_MIN_DB, default=min_default): cv.float_range(
                    min=-100, max=0
                ),
                cv.Optional(CONF_MAX_DB, default=max_default): cv.float_range(
                    min=-100, max=0
                ),
            }
        ),
        _validate_db_range,
    )


def validate_frequency_range(config: ConfigType) -> ConfigType:
    if config[CONF_MIN_FREQUENCY] >= config[CONF_MAX_FREQUENCY]:
        raise cv.Invalid(f"{CONF_MIN_FREQUENCY} must be below {CONF_MAX_FREQUENCY}")
    return config


# Options shared by the microphone analyzer and the speaker platform. ``update_interval`` only sets how often the
# sensors publish; the analysis itself runs on every audio frame.
ANALYZER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MIN_FREQUENCY, default="100Hz"): cv.All(
            cv.frequency, cv.Range(min=20, max=7000)
        ),
        cv.Optional(CONF_MAX_FREQUENCY, default="4000Hz"): cv.All(
            cv.frequency, cv.Range(min=100, max=7900)
        ),
        cv.Optional(CONF_POWER_RANGE, default={}): _db_range_schema(-70, -20),
        cv.Optional(CONF_BAND_RANGE, default={}): _db_range_schema(-80, -30),
        cv.Optional(CONF_ATTACK, default="30ms"): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_RELEASE, default="250ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.polling_component_schema("1s"))


async def setup_analyzer(var: cg.MockObj, config: ConfigType) -> None:
    """Registers the component and applies the shared analysis options."""
    await cg.register_component(var, config)
    cg.add(
        var.set_frequency_range(config[CONF_MIN_FREQUENCY], config[CONF_MAX_FREQUENCY])
    )
    power_range = config[CONF_POWER_RANGE]
    cg.add(var.set_power_range(power_range[CONF_MIN_DB], power_range[CONF_MAX_DB]))
    band_range = config[CONF_BAND_RANGE]
    cg.add(var.set_band_range(band_range[CONF_MIN_DB], band_range[CONF_MAX_DB]))
    cg.add(var.set_attack(config[CONF_ATTACK]))
    cg.add(var.set_release(config[CONF_RELEASE]))


CONFIG_SCHEMA = cv.All(
    ANALYZER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MicrophoneAudioAnalyzer),
            cv.Optional(
                CONF_MICROPHONE, default={}
            ): microphone.microphone_source_schema(
                min_bits_per_sample=16,
                max_bits_per_sample=16,
                max_channels=1,
            ),
            cv.Required(CONF_PASSIVE): cv.boolean,
        }
    ),
    validate_frequency_range,
    cv.only_on([PLATFORM_ESP32]),
)

FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Required(
            CONF_MICROPHONE
        ): microphone.final_validate_microphone_source_schema(DOMAIN),
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config: ConfigType) -> None:
    mic_source = await microphone.microphone_source_to_code(
        config[CONF_MICROPHONE], passive=config[CONF_PASSIVE]
    )
    var = cg.new_Pvariable(config[CONF_ID], mic_source)
    await setup_analyzer(var, config)
