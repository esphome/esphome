from esphome import automation, core
import esphome.codegen as cg
from esphome.components import audio, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_OUTPUT_SPEAKER,
    CONF_SAMPLE_RATE,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@kahrendt"]

CONF_OUTPUT_SPEAKERS = "output_speakers"
CONF_TARGET_SPEAKER = "target_speaker"

router_ns = cg.esphome_ns.namespace("router")
Router = router_ns.class_("Router", cg.Component, speaker.Speaker)
SwitchOutputAction = router_ns.class_("SwitchOutputAction", automation.Action)

SpeakerPtr = speaker.Speaker.operator("ptr")


def _set_stream_limits(config: ConfigType) -> ConfigType:
    # Lock the router's stream limits to the user-declared format. Limits are set
    # at CONFIG_SCHEMA time so they're visible to other components' FINAL_VALIDATE
    # (which has no guaranteed ordering vs. ours).
    audio.set_stream_limits(
        min_bits_per_sample=config[CONF_BITS_PER_SAMPLE],
        max_bits_per_sample=config[CONF_BITS_PER_SAMPLE],
        min_channels=config[CONF_NUM_CHANNELS],
        max_channels=config[CONF_NUM_CHANNELS],
        min_sample_rate=config[CONF_SAMPLE_RATE],
        max_sample_rate=config[CONF_SAMPLE_RATE],
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Router),
            cv.Required(CONF_OUTPUT_SPEAKERS): cv.All(
                cv.ensure_list(cv.use_id(speaker.Speaker)),
                cv.Length(min=2, max=8),
            ),
            # All outputs must agree on a single format so the producer can keep
            # streaming through a switch without reconfiguring. These are required
            # rather than inherited because downstream components (e.g. mixer)
            # read them from the router's declaration during FINAL_VALIDATE,
            # which can't depend on our FINAL_VALIDATE running first.
            cv.Required(CONF_BITS_PER_SAMPLE): cv.int_range(8, 32),
            cv.Required(CONF_NUM_CHANNELS): cv.int_range(1, 2),
            cv.Required(CONF_SAMPLE_RATE): cv.int_range(8000, 96000),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _set_stream_limits,
)


def _final_validate(config: ConfigType) -> ConfigType:
    # Validate every configured output speaker can accept the router's format.
    # Switching to an output that can't reproduce the format the producer is
    # already sending would otherwise fail silently at runtime.
    for spk_id in config[CONF_OUTPUT_SPEAKERS]:
        proxy = {**config, CONF_OUTPUT_SPEAKER: spk_id}
        audio.final_validate_audio_schema(
            "router",
            audio_device=CONF_OUTPUT_SPEAKER,
            bits_per_sample=config[CONF_BITS_PER_SAMPLE],
            channels=config[CONF_NUM_CHANNELS],
            sample_rate=config[CONF_SAMPLE_RATE],
        )(proxy)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # The first configured output is the default active output on boot.
    speakers = config[CONF_OUTPUT_SPEAKERS]
    cg.add(var.set_output_count(len(speakers)))
    for spk_id in speakers:
        spk = await cg.get_variable(spk_id)
        cg.add(var.add_output(spk))


@automation.register_action(
    "router.speaker.switch_output",
    SwitchOutputAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(Router),
            cv.Required(CONF_TARGET_SPEAKER): cv.templatable(
                cv.use_id(speaker.Speaker)
            ),
        }
    ),
    synchronous=True,
)
async def switch_output_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    target = config[CONF_TARGET_SPEAKER]
    if not isinstance(target, core.Lambda):
        target = await cg.get_variable(target)
    template_ = await cg.templatable(target, args, SpeakerPtr)
    cg.add(var.set_target(template_))
    return var
