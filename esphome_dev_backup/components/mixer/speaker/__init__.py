from esphome import automation
import esphome.codegen as cg
from esphome.components import audio, esp32, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_BUFFER_DURATION,
    CONF_DURATION,
    CONF_ID,
    CONF_NEVER,
    CONF_NUM_CHANNELS,
    CONF_OUTPUT_SPEAKER,
    CONF_SAMPLE_RATE,
    CONF_TASK_STACK_IN_PSRAM,
    CONF_TIMEOUT,
    PLATFORM_ESP32,
)
from esphome.core.entity_helpers import inherit_property_from
import esphome.final_validate as fv

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@kahrendt"]

mixer_speaker_ns = cg.esphome_ns.namespace("mixer_speaker")
MixerSpeaker = mixer_speaker_ns.class_("MixerSpeaker", cg.Component)
SourceSpeaker = mixer_speaker_ns.class_("SourceSpeaker", cg.Component, speaker.Speaker)

CONF_DECIBEL_REDUCTION = "decibel_reduction"
CONF_QUEUE_MODE = "queue_mode"
CONF_SOURCE_SPEAKERS = "source_speakers"

DuckingApplyAction = mixer_speaker_ns.class_(
    "DuckingApplyAction", automation.Action, cg.Parented.template(SourceSpeaker)
)


SOURCE_SPEAKER_SCHEMA = speaker.SPEAKER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SourceSpeaker),
        cv.Optional(
            CONF_BUFFER_DURATION, default="100ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TIMEOUT, default="500ms"): cv.Any(
            cv.positive_time_period_milliseconds,
            cv.one_of(CONF_NEVER, lower=True),
        ),
        cv.Optional(CONF_BITS_PER_SAMPLE, default=16): cv.int_range(16, 16),
    }
)


def _set_stream_limits(config):
    audio.set_stream_limits(
        min_bits_per_sample=16,
        max_bits_per_sample=16,
    )(config)

    return config


def _validate_source_speaker(config):
    fconf = fv.full_config.get()

    # Get ID for the output speaker and add it to the source speakrs config to easily inherit properties
    path = fconf.get_path_for_id(config[CONF_ID])[:-3]
    path.append(CONF_OUTPUT_SPEAKER)
    output_speaker_id = fconf.get_config_for_path(path)
    config[CONF_OUTPUT_SPEAKER] = output_speaker_id

    inherit_property_from(CONF_NUM_CHANNELS, CONF_OUTPUT_SPEAKER)(config)
    inherit_property_from(CONF_SAMPLE_RATE, CONF_OUTPUT_SPEAKER)(config)

    audio.final_validate_audio_schema(
        "mixer",
        audio_device=CONF_OUTPUT_SPEAKER,
        bits_per_sample=config.get(CONF_BITS_PER_SAMPLE),
        channels=config.get(CONF_NUM_CHANNELS),
        sample_rate=config.get(CONF_SAMPLE_RATE),
    )(config)

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MixerSpeaker),
            cv.Required(CONF_OUTPUT_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Required(CONF_SOURCE_SPEAKERS): cv.All(
                cv.ensure_list(SOURCE_SPEAKER_SCHEMA),
                cv.Length(min=2, max=8),
                [_set_stream_limits],
            ),
            cv.Optional(CONF_NUM_CHANNELS): cv.int_range(min=1, max=2),
            cv.Optional(CONF_QUEUE_MODE, default=False): cv.boolean,
            cv.SplitDefault(CONF_TASK_STACK_IN_PSRAM, esp32_idf=False): cv.All(
                cv.boolean, cv.only_with_esp_idf
            ),
        }
    ),
    cv.only_on([PLATFORM_ESP32]),
)

FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_SOURCE_SPEAKERS): [_validate_source_speaker],
        },
        extra=cv.ALLOW_EXTRA,
    ),
    inherit_property_from(CONF_NUM_CHANNELS, CONF_OUTPUT_SPEAKER),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    spkr = await cg.get_variable(config[CONF_OUTPUT_SPEAKER])

    cg.add(var.set_output_channels(config[CONF_NUM_CHANNELS]))
    cg.add(var.set_output_speaker(spkr))
    cg.add(var.set_queue_mode(config[CONF_QUEUE_MODE]))

    if task_stack_in_psram := config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(task_stack_in_psram))
        if task_stack_in_psram:
            if config[CONF_TASK_STACK_IN_PSRAM]:
                esp32.add_idf_sdkconfig_option(
                    "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
                )

    for speaker_config in config[CONF_SOURCE_SPEAKERS]:
        source_speaker = cg.new_Pvariable(speaker_config[CONF_ID])

        cg.add(source_speaker.set_buffer_duration(speaker_config[CONF_BUFFER_DURATION]))

        if speaker_config[CONF_TIMEOUT] != CONF_NEVER:
            cg.add(source_speaker.set_timeout(speaker_config[CONF_TIMEOUT]))

        await cg.register_component(source_speaker, speaker_config)
        await cg.register_parented(source_speaker, config[CONF_ID])
        await speaker.register_speaker(source_speaker, speaker_config)

        cg.add(var.add_source_speaker(source_speaker))


@automation.register_action(
    "mixer_speaker.apply_ducking",
    DuckingApplyAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(SourceSpeaker),
            cv.Required(CONF_DECIBEL_REDUCTION): cv.templatable(
                cv.int_range(min=0, max=51)
            ),
            cv.Optional(CONF_DURATION, default="0.0s"): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
)
async def ducking_set_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    decibel_reduction = await cg.templatable(
        config[CONF_DECIBEL_REDUCTION], args, cg.uint8
    )
    cg.add(var.set_decibel_reduction(decibel_reduction))
    duration = await cg.templatable(config[CONF_DURATION], args, cg.uint32)
    cg.add(var.set_duration(duration))
    return var
