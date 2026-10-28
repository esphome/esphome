import esphome.codegen as cg
from esphome.components import audio, esp32, psram, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_BUFFER_DURATION,
    CONF_FILTERS,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_OUTPUT_SPEAKER,
    CONF_SAMPLE_RATE,
    CONF_TASK_STACK_IN_PSRAM,
    PLATFORM_ESP32,
)
from esphome.core.entity_helpers import inherit_property_from
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_TAPS, resampler_ns, validate_taps

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@kahrendt"]

ResamplerSpeaker = resampler_ns.class_(
    "ResamplerSpeaker", cg.Component, speaker.Speaker
)

PASSTHROUGH = "passthrough"
CONF_TASK_CORE = "task_core"
CONF_TASK_PRIORITY = "task_priority"


def _set_stream_limits(config: ConfigType) -> ConfigType:
    audio.set_stream_limits(
        min_bits_per_sample=16,
        max_bits_per_sample=32,
    )(config)

    return config


def _validate_audio_compatibility(config: ConfigType) -> None:
    inherit_property_from(CONF_NUM_CHANNELS, CONF_OUTPUT_SPEAKER)(config)
    inherit_property_from(CONF_SAMPLE_RATE, CONF_OUTPUT_SPEAKER)(config)

    # In passthrough mode the output bits per sample is determined at runtime from the input stream, so there is
    # nothing to inherit or validate against the output speaker.
    passthrough = config.get(CONF_BITS_PER_SAMPLE) == PASSTHROUGH
    if not passthrough:
        inherit_property_from(CONF_BITS_PER_SAMPLE, CONF_OUTPUT_SPEAKER)(config)

    audio.final_validate_audio_schema(
        "source_speaker",
        audio_device=CONF_OUTPUT_SPEAKER,
        bits_per_sample=cv.UNDEFINED
        if passthrough
        else config.get(CONF_BITS_PER_SAMPLE),
        channels=config.get(CONF_NUM_CHANNELS),
        sample_rate=config.get(CONF_SAMPLE_RATE),
    )(config)


def _validate_task_core(config: ConfigType) -> ConfigType:
    if config[CONF_TASK_CORE] != 1:
        return config
    esp32.only_on_variant(
        supported=[
            esp32.VARIANT_ESP32,
            esp32.VARIANT_ESP32S3,
            esp32.VARIANT_ESP32P4,
            esp32.VARIANT_ESP32H4,
            esp32.VARIANT_ESP32S31,
        ],
        msg_prefix="resampler speaker task_core: 1",
    )(config)
    sdkconfig = fv.full_config.get()[PLATFORM_ESP32][CONF_FRAMEWORK][
        esp32.CONF_SDKCONFIG_OPTIONS
    ]
    if sdkconfig.get("CONFIG_FREERTOS_UNICORE") == "y":
        raise cv.Invalid("task_core: 1 requires FreeRTOS dual-core mode")
    return config


CONFIG_SCHEMA = cv.All(
    speaker.SPEAKER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ResamplerSpeaker),
            cv.Required(CONF_OUTPUT_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Optional(CONF_BITS_PER_SAMPLE, default=PASSTHROUGH): cv.Any(
                cv.one_of(PASSTHROUGH, lower=True), cv.int_range(8, 32)
            ),
            cv.Optional(
                CONF_BUFFER_DURATION, default="100ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): psram.validate_task_stack_in_psram,
            cv.Optional(CONF_TASK_CORE, default=-1): cv.int_range(min=-1, max=1),
            cv.Optional(CONF_TASK_PRIORITY, default=1): cv.int_range(min=1, max=23),
            cv.Optional(CONF_FILTERS, default=16): cv.int_range(min=2, max=1024),
            cv.Optional(CONF_TAPS, default=16): validate_taps,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32]),
    _set_stream_limits,
)


FINAL_VALIDATE_SCHEMA = cv.All(_validate_task_core, _validate_audio_compatibility)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await speaker.register_speaker(var, config)

    await cg.add_variable(var.set_output_speaker, config[CONF_OUTPUT_SPEAKER])

    cg.add(var.set_buffer_duration(config[CONF_BUFFER_DURATION]))
    cg.add(
        var.set_task_scheduling(
            cg.TemplateArguments(config[CONF_TASK_CORE], config[CONF_TASK_PRIORITY])
        )
    )

    if config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(True))
        psram.request_external_task_stack()

    if config[CONF_BITS_PER_SAMPLE] == PASSTHROUGH:
        cg.add(var.set_passthrough_bits_per_sample(True))
    else:
        cg.add(var.set_target_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))
    cg.add(var.set_target_sample_rate(config[CONF_SAMPLE_RATE]))

    cg.add(var.set_filters(config[CONF_FILTERS]))
    cg.add(var.set_taps(config[CONF_TAPS]))
