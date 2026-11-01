"""ESP-SR echo-cancelled microphone for a full-duplex I2S bus."""

import esphome.codegen as cg
from esphome.components import audio, esp32, i2s_audio, logger, microphone
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_CHANNEL,
    CONF_ID,
    CONF_LEVEL,
    CONF_LOGGER,
    CONF_MICROPHONE,
    CONF_SAMPLE_RATE,
    CONF_SPEAKER,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

DOMAIN = "i2s_aec_microphone"
CODEOWNERS = ["@mbrudka"]
DEPENDENCIES = ["i2s_audio", "microphone"]
AUTO_LOAD = ["audio"]

CONF_I2S_AUDIO_ID = "i2s_audio_id"
CONF_TASK_CORE = "task_core"
CONF_FILTER_LENGTH = "filter_length"
CONF_REFERENCE_GAIN = "reference_gain"
CONF_FAST_REFERENCE_DECIMATION = "fast_reference_decimation"

aec_ns = cg.esphome_ns.namespace(DOMAIN)
I2SAECMicrophone = aec_ns.class_(
    "I2SAECMicrophone", cg.Component, microphone.Microphone
)

CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(I2SAECMicrophone),
            cv.GenerateID(CONF_I2S_AUDIO_ID): cv.use_id(i2s_audio.I2SAudioComponent),
            cv.Required(CONF_MICROPHONE): cv.use_id(microphone.Microphone),
            cv.Optional(CONF_TASK_CORE): cv.one_of(0, 1, int=True),
            cv.Optional(CONF_FILTER_LENGTH, default=2): cv.int_range(min=1, max=4),
            cv.Optional(CONF_REFERENCE_GAIN, default=1.0): cv.float_range(
                min=0.1, max=20.0
            ),
            cv.Optional(CONF_FAST_REFERENCE_DECIMATION, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(["esp32"]),
    cv.only_with_framework("esp-idf"),
    esp32.only_on_variant(supported=[esp32.VARIANT_ESP32]),
)


def _set_stream_limits(config: dict) -> dict:
    audio.set_stream_limits(
        min_bits_per_sample=16,
        max_bits_per_sample=16,
        min_channels=1,
        max_channels=1,
        min_sample_rate=16000,
        max_sample_rate=16000,
    )(config)
    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _set_stream_limits)


def _final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    if "psram" not in full_config:
        raise cv.Invalid("i2s_aec_microphone currently requires PSRAM")
    buses = full_config["i2s_audio"]
    bus_id = str(config[CONF_I2S_AUDIO_ID])
    if not any(
        str(bus[CONF_ID]) == bus_id
        and bus["full_duplex"]
        and bus[i2s_audio.CONF_KEEP_TX_ACTIVE]
        for bus in buses
    ):
        raise cv.Invalid(
            "i2s_aec_microphone requires a full-duplex I2S bus with keep_tx_active enabled"
        )

    speakers = [
        speaker
        for speaker in full_config.get(CONF_SPEAKER, [])
        if speaker.get("platform") == "i2s_audio"
        and str(speaker.get(CONF_I2S_AUDIO_ID)) == bus_id
    ]
    if config[CONF_FAST_REFERENCE_DECIMATION] and any(
        speaker[CONF_SAMPLE_RATE] != 48000 for speaker in speakers
    ):
        raise cv.Invalid(
            "fast_reference_decimation requires a 48 kHz I2S speaker for 48-to-16 kHz decimation"
        )
    if not speakers or any(
        speaker[CONF_SAMPLE_RATE] not in (16000, 48000)
        or speaker[CONF_BITS_PER_SAMPLE] != 16
        or speaker[CONF_CHANNEL] != "stereo"
        for speaker in speakers
    ):
        raise cv.Invalid(
            "i2s_aec_microphone requires 16 or 48 kHz, 16-bit stereo I2S output"
        )

    microphones = {
        str(mic[CONF_ID]): mic for mic in full_config.get(CONF_MICROPHONE, [])
    }
    source = config[CONF_MICROPHONE]
    for _ in range(len(microphones) + 1):
        if not isinstance(source, dict):
            source = microphones.get(str(source))
        if source is None:
            break
        if source.get("platform") == "i2s_audio":
            if str(source.get(CONF_I2S_AUDIO_ID)) == bus_id:
                return
            break
        parent = source.get(CONF_MICROPHONE)
        if isinstance(parent, dict):
            parent = parent.get(CONF_MICROPHONE)
        source = parent
    raise cv.Invalid("The AEC source must originate from a microphone on its I2S bus")


FINAL_VALIDATE_SCHEMA = cv.All(
    audio.final_validate_audio_schema(
        "i2s_aec_microphone",
        audio_device=CONF_MICROPHONE,
        bits_per_sample=16,
        channels=1,
        sample_rate=16000,
        audio_device_issue=True,
    ),
    _final_validate,
)


async def to_code(config: ConfigType) -> None:
    esp32.add_idf_component(name="espressif/esp-sr", ref="2.5.5")
    if config[CONF_FAST_REFERENCE_DECIMATION]:
        esp32.add_idf_component(name="espressif/esp-dsp", ref="~1.8.0")
        cg.add_define("USE_DSP_DECIMATOR")
        cg.add_define("USE_I2S_AEC_MICROPHONE_DSP_DECIMATOR")
    # The build level selects the diagnostic work. A tag's runtime level only
    # controls whether its results are printed.
    logger_config = CORE.config.get(CONF_LOGGER)
    if logger_config is not None and logger.LOG_LEVEL_SEVERITY.index(
        logger_config[CONF_LEVEL]
    ) >= logger.LOG_LEVEL_SEVERITY.index("DEBUG"):
        cg.add_define("USE_I2S_AEC_MICROPHONE_DIAGNOSTICS")
    cg.add_define("USE_I2S_AEC_MICROPHONE")
    cg.add_define("USE_I2S_AUDIO_TX_DMA_OBSERVER")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)
    source = await cg.get_variable(config[CONF_MICROPHONE])
    bus = await cg.get_variable(config[CONF_I2S_AUDIO_ID])
    cg.add(var.set_source(source))
    cg.add(var.set_bus(bus))
    speaker_config = next(
        speaker
        for speaker in CORE.config[CONF_SPEAKER]
        if speaker.get("platform") == "i2s_audio"
        and str(speaker.get(CONF_I2S_AUDIO_ID)) == str(config[CONF_I2S_AUDIO_ID])
    )
    cg.add(var.set_reference_sample_rate(speaker_config[CONF_SAMPLE_RATE]))
    if config[CONF_FAST_REFERENCE_DECIMATION]:
        cg.add(var.set_fast_reference_decimation(True))
    if (task_core := config.get(CONF_TASK_CORE)) is not None:
        cg.add(var.set_task_core(task_core))
    cg.add(var.set_filter_length(config[CONF_FILTER_LENGTH]))
    cg.add(var.set_reference_gain_hundredths(round(100 * config[CONF_REFERENCE_GAIN])))
