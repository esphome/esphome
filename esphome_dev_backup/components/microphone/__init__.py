from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import audio
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_CHANNELS,
    CONF_GAIN_FACTOR,
    CONF_ID,
    CONF_MICROPHONE,
    CONF_TRIGGER_ID,
)
from esphome.core import CORE
from esphome.coroutine import coroutine_with_priority

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@jesserockz", "@kahrendt"]

IS_PLATFORM_COMPONENT = True

CONF_ON_DATA = "on_data"

microphone_ns = cg.esphome_ns.namespace("microphone")

Microphone = microphone_ns.class_("Microphone")
MicrophoneSource = microphone_ns.class_("MicrophoneSource")

CaptureAction = microphone_ns.class_(
    "CaptureAction", automation.Action, cg.Parented.template(Microphone)
)
StopCaptureAction = microphone_ns.class_(
    "StopCaptureAction", automation.Action, cg.Parented.template(Microphone)
)
MuteAction = microphone_ns.class_(
    "MuteAction", automation.Action, cg.Parented.template(Microphone)
)
UnmuteAction = microphone_ns.class_(
    "UnmuteAction", automation.Action, cg.Parented.template(Microphone)
)


DataTrigger = microphone_ns.class_(
    "DataTrigger",
    automation.Trigger.template(cg.std_vector.template(cg.uint8).operator("ref")),
)

IsCapturingCondition = microphone_ns.class_(
    "IsCapturingCondition", automation.Condition
)
IsMutedCondition = microphone_ns.class_("IsMutedCondition", automation.Condition)


async def setup_microphone_core_(var, config):
    for conf in config.get(CONF_ON_DATA, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [(cg.std_vector.template(cg.uint8).operator("ref").operator("const"), "x")],
            conf,
        )


async def register_microphone(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    await setup_microphone_core_(var, config)


MICROPHONE_SCHEMA = cv.Schema.extend(audio.AUDIO_COMPONENT_SCHEMA).extend(
    {
        cv.Optional(CONF_ON_DATA): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DataTrigger),
            }
        ),
    }
)


MICROPHONE_ACTION_SCHEMA = maybe_simple_id({cv.GenerateID(): cv.use_id(Microphone)})


def microphone_source_schema(
    min_bits_per_sample: int = 16,
    max_bits_per_sample: int = 16,
    min_channels: int = 1,
    max_channels: int = 1,
):
    """Schema for a microphone source

    Components requesting microphone data should use this schema instead of accessing a microphone directly.

    Args:
      min_bits_per_sample (int, optional): Minimum number of bits per sample the requesting component supports. Defaults to 16.
      max_bits_per_sample (int, optional): Maximum number of bits per sample the requesting component supports. Defaults to 16.
      min_channels (int, optional): Minimum number of channels the requesting component supports. Defaults to 1.
      max_channels (int, optional): Maximum number of channels the requesting component supports. Defaults to 1.
    """

    def _validate_unique_channels(config):
        if len(config) != len(set(config)):
            raise cv.Invalid("Channels must be unique")
        return config

    return cv.All(
        automation.maybe_conf(
            CONF_MICROPHONE,
            {
                cv.GenerateID(CONF_ID): cv.declare_id(MicrophoneSource),
                cv.GenerateID(CONF_MICROPHONE): cv.use_id(Microphone),
                cv.Optional(CONF_BITS_PER_SAMPLE, default=16): cv.int_range(
                    min_bits_per_sample, max_bits_per_sample
                ),
                cv.Optional(CONF_CHANNELS, default="0"): cv.All(
                    cv.ensure_list(cv.int_range(0, 7)),
                    cv.Length(min=min_channels, max=max_channels),
                    _validate_unique_channels,
                ),
                cv.Optional(CONF_GAIN_FACTOR, default="1"): cv.int_range(1, 64),
            },
        ),
    )


def final_validate_microphone_source_schema(
    component_name: str, sample_rate: int = cv.UNDEFINED
):
    """Validates that the microphone source can provide audio in the correct format. In particular it validates the sample rate and the enabled channels.

    Note that:
      - MicrophoneSource class automatically handles converting bits per sample, so no need to validate
      - microphone_source_schema already validates that channels are unique and specifies the max number of channels the component supports

    Args:
        component_name (str): The name of the component requesting mic audio
        sample_rate (int, optional): The sample rate the component requesting mic audio requires
    """

    def _validate_audio_compatability(config):
        if sample_rate is not cv.UNDEFINED:
            # Issues require changing the microphone configuration
            #  - Verifies sample rates match
            audio.final_validate_audio_schema(
                component_name,
                audio_device=CONF_MICROPHONE,
                sample_rate=sample_rate,
                audio_device_issue=True,
            )(config)

        # Issues require changing the MicrophoneSource configuration
        # - Verifies that each of the enabled channels are available
        audio.final_validate_audio_schema(
            component_name,
            audio_device=CONF_MICROPHONE,
            enabled_channels=config[CONF_CHANNELS],
            audio_device_issue=False,
        )(config)

        return config

    return _validate_audio_compatability


async def microphone_source_to_code(config, passive=False):
    """Creates a MicrophoneSource variable for codegen.

    Setting passive to true makes the MicrophoneSource never start/stop the microphone, but only receives audio when another component has actively started the Microphone. If false, then the microphone needs to be explicitly started/stopped.

    Args:
        config (Schema): Created with `microphone_source_schema` specifying bits per sample, channels, and gain factor
        passive (bool): Enable passive mode for the MicrophoneSource
    """
    mic = await cg.get_variable(config[CONF_MICROPHONE])
    mic_source = cg.new_Pvariable(
        config[CONF_ID],
        mic,
        config[CONF_BITS_PER_SAMPLE],
        config[CONF_GAIN_FACTOR],
        passive,
    )
    for channel in config[CONF_CHANNELS]:
        cg.add(mic_source.add_channel(channel))
    return mic_source


async def microphone_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


automation.register_action(
    "microphone.capture", CaptureAction, MICROPHONE_ACTION_SCHEMA
)(microphone_action)

automation.register_action(
    "microphone.stop_capture", StopCaptureAction, MICROPHONE_ACTION_SCHEMA
)(microphone_action)

automation.register_action("microphone.mute", MuteAction, MICROPHONE_ACTION_SCHEMA)(
    microphone_action
)
automation.register_action("microphone.unmute", UnmuteAction, MICROPHONE_ACTION_SCHEMA)(
    microphone_action
)

automation.register_condition(
    "microphone.is_capturing", IsCapturingCondition, MICROPHONE_ACTION_SCHEMA
)(microphone_action)
automation.register_condition(
    "microphone.is_muted", IsMutedCondition, MICROPHONE_ACTION_SCHEMA
)(microphone_action)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(microphone_ns.using)
    cg.add_define("USE_MICROPHONE")
