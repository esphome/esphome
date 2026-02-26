from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIMEOUT

watchdog_ns = cg.esphome_ns.namespace("watchdog")
WatchdogManagerComponent = watchdog_ns.class_("WatchdogManagerComponent", cg.Component)

CODEOWNERS = ["@oarcher"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WatchdogManagerComponent),
        cv.Optional(CONF_TIMEOUT): cv.All(
            cv.Any(cv.only_on_esp32, cv.only_on_rp2040),
            cv.positive_not_null_time_period,
            cv.positive_time_period_milliseconds,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if timeout_ms := config.get(CONF_TIMEOUT):
        cg.add(var.set_timeout(timeout_ms))


# Action
TimeoutAction = watchdog_ns.class_(
    "WatchdogManagerComponentTimeoutAction",
    automation.Action,
    cg.Parented.template(WatchdogManagerComponent),
)


TIMEOUT_ACTION_SCHEMA = automation.maybe_conf(
    CONF_TIMEOUT,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(WatchdogManagerComponent),
            cv.Required(CONF_TIMEOUT): cv.All(
                cv.Any(cv.only_on_esp32, cv.only_on_rp2040),
                cv.positive_not_null_time_period,
                cv.positive_time_period_milliseconds,
            ),
        }
    ),
)


@automation.register_action(
    "watchdog.set_timeout", TimeoutAction, TIMEOUT_ACTION_SCHEMA
)
async def coap_client_request_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_timeout_ms(config.get(CONF_TIMEOUT)))

    return var
