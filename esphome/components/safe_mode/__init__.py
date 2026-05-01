from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISABLED,
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_REBOOT_TIMEOUT,
    CONF_SAFE_MODE,
    KEY_PAST_SAFE_MODE,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.cpp_generator import RawExpression

CODEOWNERS = ["@paulmonigatti", "@jsuanet", "@kbx81"]

CONF_BOOT_IS_GOOD_AFTER = "boot_is_good_after"
CONF_ON_SAFE_MODE = "on_safe_mode"

safe_mode_ns = cg.esphome_ns.namespace("safe_mode")
SafeModeComponent = safe_mode_ns.class_("SafeModeComponent", cg.Component)
MarkSuccessfulAction = safe_mode_ns.class_("MarkSuccessfulAction", automation.Action)


def _remove_id_if_disabled(value):
    value = value.copy()
    if value[CONF_DISABLED]:
        value.pop(CONF_ID)
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SafeModeComponent),
            cv.Optional(
                CONF_BOOT_IS_GOOD_AFTER, default="1min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DISABLED, default=False): cv.boolean,
            cv.Optional(CONF_NUM_ATTEMPTS, default="10"): cv.positive_not_null_int,
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="5min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_SAFE_MODE): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _remove_id_if_disabled,
)


@automation.register_action(
    "safe_mode.mark_successful",
    MarkSuccessfulAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(SafeModeComponent),
        }
    ),
    synchronous=True,
)
async def safe_mode_mark_successful_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parent(parent))
    return var


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_SAFE_MODE, "add_on_safe_mode_callback"),
)


@coroutine_with_priority(CoroPriority.APPLICATION)
async def to_code(config):
    if not config[CONF_DISABLED]:
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)

        if on_safe_mode := config.get(CONF_ON_SAFE_MODE):
            cg.add_define("USE_SAFE_MODE_CALLBACK")
            cg.add_define("ESPHOME_SAFE_MODE_CALLBACK_COUNT", len(on_safe_mode))
            await automation.build_callback_automations(
                var, config, _CALLBACK_AUTOMATIONS
            )

        condition = var.should_enter_safe_mode(
            config[CONF_NUM_ATTEMPTS],
            config[CONF_REBOOT_TIMEOUT],
            config[CONF_BOOT_IS_GOOD_AFTER],
        )
        cg.add(RawExpression(f"if ({condition}) return"))

    CORE.data[CONF_SAFE_MODE] = {}
    CORE.data[CONF_SAFE_MODE][KEY_PAST_SAFE_MODE] = True
