import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart, web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import CONF_ID, CONF_CURRENT, CONF_TRIGGER_ID, CONF_UPDATE_INTERVAL


DEPENDENCIES = ["uart"]
AUTO_LOAD = ["web_server_base"]

CONF_SIMPLEEVSE_ID = "simpleevse_id"
CONF_ON_UNPLUGGED = "on_unplugged"
CONF_ON_PLUGGED = "on_plugged"
CONF_ENABLE = "enable"
CONF_WEB_CONFIG_ID = "web_config_id"
CONF_WEB_CONFIG = "web_config"

# Base component and web server integration
simpleevse_ns = cg.esphome_ns.namespace("simpleevse")
SimpleEvseComponent = simpleevse_ns.class_(
    "SimpleEvseComponent", cg.Component, uart.UARTDevice
)
SimpleEvseHttpHandler = simpleevse_ns.class_("SimpleEvseHttpHandler", cg.Component)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SimpleEvseComponent),
            cv.Optional(CONF_UPDATE_INTERVAL, default="5s"): cv.update_interval,
            cv.Optional(CONF_WEB_CONFIG, default=False): cv.boolean,
            cv.GenerateID(key=CONF_WEB_CONFIG_ID): cv.declare_id(SimpleEvseHttpHandler),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.Optional(CONF_ON_UNPLUGGED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        automation.Trigger.template()
                    ),
                }
            ),
            cv.Optional(CONF_ON_PLUGGED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        automation.Trigger.template()
                    ),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


def to_code(config):
    simpleevse = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(simpleevse, config)
    yield uart.register_uart_device(simpleevse, config)

    if config.get(CONF_WEB_CONFIG):
        web_server = yield cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
        handler = cg.new_Pvariable(config[CONF_WEB_CONFIG_ID], web_server, simpleevse)
        yield cg.register_component(handler, {})
        cg.add_define("USE_SIMPLEEVSE_WEB_CONFIG")

    for conf in config.get(CONF_ON_UNPLUGGED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        yield automation.build_automation(trigger, [], conf)
        cg.add(simpleevse.set_unplugged_trigger(trigger))

    for conf in config.get(CONF_ON_PLUGGED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        yield automation.build_automation(trigger, [], conf)
        cg.add(simpleevse.set_plugged_trigger(trigger))


# Actions
SetChargingCurrentAction = simpleevse_ns.class_("SetChargingCurrent", automation.Action)
SetChargingEnabled = simpleevse_ns.class_("SetChargingEnabled", automation.Action)


@automation.register_action(
    "simpleevse.set_charging_current",
    SetChargingCurrentAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(SimpleEvseComponent),
            cv.Required(CONF_CURRENT): cv.templatable(cv.uint8_t),
        }
    ),
)
def set_charging_current_to_code(config, action_id, template_arg, args):
    evse = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, evse)
    template_ = yield cg.templatable(config[CONF_CURRENT], args, cg.uint8)
    cg.add(var.set_current(template_))
    yield var


@automation.register_action(
    "simpleevse.set_charging_enabled",
    SetChargingEnabled,
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(SimpleEvseComponent),
            cv.Required(CONF_ENABLE): cv.templatable(cv.boolean),
        }
    ),
)
def set_charging_enabled_to_code(config, action_id, template_arg, args):
    evse = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, evse)
    template_ = yield cg.templatable(config[CONF_ENABLE], args, cg.bool_)
    cg.add(var.set_enabled(template_))
    yield var


# Condition
PluggedCondition = simpleevse_ns.class_("PluggedCondition", automation.Condition)

PLUGGED_CONDITION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(CONF_ID): cv.use_id(SimpleEvseComponent),
    }
)


@automation.register_condition(
    "simpleevse.is_plugged", PluggedCondition, PLUGGED_CONDITION_SCHEMA
)
def plugged_to_code(config, condition_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(condition_id, template_arg, paren)


UnluggedCondition = simpleevse_ns.class_("UnluggedCondition", automation.Condition)

UNPLUGGED_CONDITION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(CONF_ID): cv.use_id(SimpleEvseComponent),
    }
)


@automation.register_condition(
    "simpleevse.is_unplugged", UnluggedCondition, UNPLUGGED_CONDITION_SCHEMA
)
def unplugged_to_code(config, condition_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(condition_id, template_arg, paren)
