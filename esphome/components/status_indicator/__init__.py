import esphome.config_validation as cv
import esphome.codegen as cg
import esphome.automation as auto
from esphome.const import (
    CONF_ID,
    CONF_PRIORITY,
    CONF_GROUP,
    CONF_TRIGGER_ID,
    CONF_ON_TURN_OFF,
)

from esphome.core import coroutine_with_priority

CODEOWNERS = ["@nielsnl68"]

status_led_ns = cg.esphome_ns.namespace("status_indicator")
StatusIndicator = status_led_ns.class_("StatusIndicator", cg.Component)
StatusTrigger = status_led_ns.class_("StatusTrigger", auto.Trigger.template())
StatusAction = status_led_ns.class_("StatusAction", auto.Trigger.template())
CONF_TRIGGER_LIST = {
    "on_app_error": True,
    "on_clear_app_error": True,
    "on_app_warning": True,
    "on_clear_app_warning": True,
    "on_network_connected": True,
    "on_network_disconnected": True,
    "on_wifi_ap_enabled": True,
    "on_wifi_ap_disabled": True,
    "on_api_connected": True,
    "on_api_disconnected": True,
    "on_mqtt_connected": True,
    "on_mgtt_disconnected": True,
    "on_custom_status": False,
}
CONF_WHICH = "which"


def trigger_setup(Single):
    return auto.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StatusTrigger),
            cv.Optional(CONF_GROUP, default=""): cv.string,
            cv.Optional(CONF_PRIORITY, default=0): cv.int_range(0, 10),
        },
        single=Single,
    )


def add_default_triggers():
    result = {}

    for trigger, single in CONF_TRIGGER_LIST.items():
        result[cv.Optional(trigger)] = trigger_setup(single)
    return cv.Schema(result)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(StatusIndicator),
            cv.Required(CONF_ON_TURN_OFF): trigger_setup(True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(add_default_triggers())
)


async def add_trigger(var, conf, key):
    trigger = cg.new_Pvariable(
        conf[CONF_TRIGGER_ID],
        var,
        conf[CONF_GROUP],
        conf[CONF_PRIORITY],
    )
    await auto.build_automation(trigger, [], conf)
    if key is not None:
        cg.add(var.set_trigger(key, trigger))


@coroutine_with_priority(80.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await add_trigger(var, config.get(CONF_ON_TURN_OFF), CONF_ON_TURN_OFF)

    for trigger_name, single in CONF_TRIGGER_LIST.items():
        conf = config.get(trigger_name, None)
        if conf is not None:
            if single:
                await add_trigger(var, conf, trigger_name)
            else:
                for conf in config.get(trigger_name, []):
                    await add_trigger(var, conf, None)


@auto.register_action(
    "status.push",
    StatusAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(CONF_ID): cv.use_id(StatusIndicator),
            cv.Required(CONF_TRIGGER_ID): cv.use_id(StatusTrigger),
        },
        key=CONF_TRIGGER_ID,
    ),
)
async def status_action_push_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_state(True))
    cg.add(var.set_trigger(config[CONF_TRIGGER_ID]))
    return var


@auto.register_action(
    "status.pop",
    StatusAction,
    cv.All(
        cv.maybe_simple_value(
            {
                cv.GenerateID(CONF_ID): cv.use_id(StatusIndicator),
                cv.Optional(CONF_TRIGGER_ID): cv.use_id(StatusTrigger),
                cv.Optional(CONF_WHICH): cv.string,
            },
            key=CONF_TRIGGER_ID,
        ),
    ),
)
async def status_action_pop_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_state(False))
    if CONF_TRIGGER_ID in config:
        cg.add(var.set_group(config[CONF_TRIGGER_ID]))
    else:
        cg.add(var.set_trigger(config[CONF_WHICH]))

    return var
