from esphome import automation
import esphome.codegen as cg
from esphome.components import media_source
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_SAMPLE_RATE,
    CONF_TASK_STACK_IN_PSRAM,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

from .. import (
    CONF_DECODE_MEMORY,
    CONF_FIXED_DELAY,
    CONF_INITIAL_STATIC_DELAY,
    CONF_SENDSPIN_ID,
    MEMORY_LOCATIONS,
    SendspinHub,
    _validate_task_stack_in_psram,
    register_player_config,
    request_controller_support,
    sendspin_ns,
)

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@kahrendt"]

CONF_STATIC_DELAY_ADJUSTABLE = "static_delay_adjustable"


SendspinMediaSource = sendspin_ns.class_(
    "SendspinMediaSource",
    cg.Component,
    media_source.MediaSource,
)

EnableStaticDelayAdjustmentAction = sendspin_ns.class_(
    "EnableStaticDelayAdjustmentAction",
    automation.Action,
    cg.Parented.template(SendspinMediaSource),
)

DisableStaticDelayAdjustmentAction = sendspin_ns.class_(
    "DisableStaticDelayAdjustmentAction",
    automation.Action,
    cg.Parented.template(SendspinMediaSource),
)


def _register(config: ConfigType) -> ConfigType:
    request_controller_support()
    register_player_config(
        {
            CONF_SAMPLE_RATE: config[CONF_SAMPLE_RATE],
            CONF_BUFFER_SIZE: config[CONF_BUFFER_SIZE],
            CONF_INITIAL_STATIC_DELAY: config[CONF_INITIAL_STATIC_DELAY],
            CONF_FIXED_DELAY: config[CONF_FIXED_DELAY],
            CONF_TASK_STACK_IN_PSRAM: config.get(CONF_TASK_STACK_IN_PSRAM, False),
            CONF_DECODE_MEMORY: config.get(CONF_DECODE_MEMORY),
        }
    )
    return config


CONFIG_SCHEMA = cv.All(
    media_source.media_source_schema(
        SendspinMediaSource,
    ).extend(
        {
            cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): _validate_task_stack_in_psram,
            cv.Optional(CONF_BUFFER_SIZE, default=1000000): cv.int_range(min=25000),
            cv.Optional(CONF_INITIAL_STATIC_DELAY, default="0ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(milliseconds=5000)),
            ),
            cv.Optional(CONF_STATIC_DELAY_ADJUSTABLE, default=False): cv.boolean,
            cv.Optional(CONF_FIXED_DELAY, default="0us"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=cv.TimePeriod(microseconds=10000)),
            ),
            cv.Optional(CONF_SAMPLE_RATE, default=48000): cv.int_range(
                min=16000, max=96000
            ),
            cv.Optional(CONF_DECODE_MEMORY): cv.one_of(*MEMORY_LOCATIONS, lower=True),
        }
    ),
    cv.only_on_esp32,
    _register,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_source.register_media_source(var, config)

    sendspin_hub = await cg.get_variable(config[CONF_SENDSPIN_ID])
    await cg.register_parented(var, sendspin_hub)

    cg.add(sendspin_hub.set_listener(var))

    cg.add(var.set_static_delay_adjustable(config[CONF_STATIC_DELAY_ADJUSTABLE]))


SENDSPIN_MEDIA_SOURCE_ACTION_SCHEMA = automation.maybe_simple_id(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(SendspinMediaSource),
        }
    )
)


@automation.register_action(
    "sendspin.media_source.enable_static_delay_adjustment",
    EnableStaticDelayAdjustmentAction,
    SENDSPIN_MEDIA_SOURCE_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "sendspin.media_source.disable_static_delay_adjustment",
    DisableStaticDelayAdjustmentAction,
    SENDSPIN_MEDIA_SOURCE_ACTION_SCHEMA,
    synchronous=True,
)
async def sendspin_static_delay_adjustment_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
