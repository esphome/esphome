import esphome.codegen as cg
from esphome.components import audio, microphone, psram
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_DURATION,
    CONF_CHANNELS,
    CONF_FILTERS,
    CONF_ID,
    CONF_MICROPHONE,
    CONF_SAMPLE_RATE,
    CONF_TASK_STACK_IN_PSRAM,
    PLATFORM_ESP32,
)

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@kyvaith"]

resampler_ns = cg.esphome_ns.namespace("resampler")
ResamplerMicrophone = resampler_ns.class_(
    "ResamplerMicrophone", cg.Component, microphone.Microphone
)

CONF_TAPS = "taps"


def _set_stream_limits(config):
    audio.set_stream_limits(
        max_channels=len(config[CONF_MICROPHONE][CONF_CHANNELS]),
        min_sample_rate=config[CONF_SAMPLE_RATE],
        max_sample_rate=config[CONF_SAMPLE_RATE],
    )(config)
    return config


def _validate_taps(taps):
    value = cv.int_range(min=16, max=128)(taps)
    if value % 4 != 0:
        raise cv.Invalid("Number of taps must be divisible by 4")
    return value


CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ResamplerMicrophone),
            cv.Required(CONF_MICROPHONE): microphone.microphone_source_schema(
                min_channels=1,
                max_channels=3,
            ),
            cv.Optional(
                CONF_BUFFER_DURATION, default="100ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): psram.validate_task_stack_in_psram,
            cv.Optional(CONF_FILTERS, default=16): cv.int_range(min=2, max=1024),
            cv.Optional(CONF_TAPS, default=16): _validate_taps,
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(min=1),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32]),
    _set_stream_limits,
)

FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Required(
            CONF_MICROPHONE
        ): microphone.final_validate_microphone_source_schema("resampler_microphone"),
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)

    mic_source = await microphone.microphone_source_to_code(config[CONF_MICROPHONE])
    cg.add(var.set_microphone_source(mic_source))

    cg.add(var.set_buffer_duration(config[CONF_BUFFER_DURATION]))

    if config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(True))
        psram.request_external_task_stack()

    cg.add(var.set_target_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_filters(config[CONF_FILTERS]))
    cg.add(var.set_taps(config[CONF_TAPS]))
