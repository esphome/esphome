"""Sendspin media source component for ESPHome."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import media_source, psram
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID, CONF_TASK_STACK_IN_PSRAM

from .. import CONF_SENDSPIN_ID, SendspinHub, sendspin_ns

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["media_source", "audio"]

CONF_INITIAL_STATIC_DELAY = "initial_static_delay"
CONF_STATIC_DELAY_ADJUSTABLE = "static_delay_adjustable"
CONF_FIXED_DELAY = "fixed_delay"

SendspinMediaSource = sendspin_ns.class_(
    "SendspinMediaSource",
    cg.Component,
    media_source.MediaSource,
)

EnableStaticDelayAdjustmentAction = sendspin_ns.class_(
    "EnableStaticDelayAdjustmentAction",
    automation.Action,
    cg.Parented.template(SendspinHub),
)

DisableStaticDelayAdjustmentAction = sendspin_ns.class_(
    "DisableStaticDelayAdjustmentAction",
    automation.Action,
    cg.Parented.template(SendspinHub),
)

CONFIG_SCHEMA = (
    media_source.media_source_schema(
        SendspinMediaSource,
    )
    .extend(
        {
            cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): cv.All(
                cv.boolean, cv.requires_component(psram.DOMAIN)
            ),
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
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    """Generate code for sendspin media source."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_source.register_media_source(var, config)

    sendspin_hub = await cg.get_variable(config[CONF_SENDSPIN_ID])
    await cg.register_parented(var, sendspin_hub)

    cg.add(sendspin_hub.set_buffer_size(config[CONF_BUFFER_SIZE]))

    if config[CONF_INITIAL_STATIC_DELAY].total_milliseconds > 0:
        cg.add(
            sendspin_hub.set_initial_static_delay_ms(config[CONF_INITIAL_STATIC_DELAY])
        )

    if config[CONF_STATIC_DELAY_ADJUSTABLE]:
        cg.add(sendspin_hub.set_static_delay_adjustable(True))

    if config[CONF_FIXED_DELAY].total_microseconds > 0:
        cg.add(sendspin_hub.set_fixed_delay_us(config[CONF_FIXED_DELAY]))

    cg.add_define("USE_SENDSPIN_PLAYER")
    cg.add_define("USE_SENDSPIN_CONTROLLER")

    if CONF_TASK_STACK_IN_PSRAM in config:
        cg.add(var.set_task_stack_in_psram(config[CONF_TASK_STACK_IN_PSRAM]))


SENDSPIN_SIMPLE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SendspinHub),
    }
)


@automation.register_action(
    "sendspin.media_source.enable_static_delay_adjustment",
    EnableStaticDelayAdjustmentAction,
    SENDSPIN_SIMPLE_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "sendspin.media_source.disable_static_delay_adjustment",
    DisableStaticDelayAdjustmentAction,
    SENDSPIN_SIMPLE_ACTION_SCHEMA,
    synchronous=True,
)
async def sendspin_static_delay_adjustment_to_code(
    config, action_id, template_arg, args
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
