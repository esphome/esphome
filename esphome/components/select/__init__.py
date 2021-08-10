from typing import List
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.const import (
    CONF_DISABLED_BY_DEFAULT,
    CONF_ICON,
    CONF_ID,
    CONF_INTERNAL,
    CONF_ON_VALUE,
    CONF_OPTION,
    CONF_TRIGGER_ID,
    CONF_NAME,
    CONF_MQTT_ID,
    ICON_EMPTY,
)
from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True

select_ns = cg.esphome_ns.namespace("select")
Select = select_ns.class_("Select", cg.Nameable)
SelectPtr = Select.operator("ptr")

# Triggers
SelectStateTrigger = select_ns.class_(
    "SelectStateTrigger", automation.Trigger.template(cg.float_)
)

# Actions
SelectSetAction = select_ns.class_("SelectSetAction", automation.Action)

icon = cv.icon


SELECT_SCHEMA = cv.NAMEABLE_SCHEMA.extend(cv.MQTT_COMPONENT_SCHEMA).extend(
    {
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTSelectComponent),
        cv.GenerateID(): cv.declare_id(Select),
        cv.Optional(CONF_ICON, default=ICON_EMPTY): icon,
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SelectStateTrigger),
            }
        ),
    }
)


async def setup_select_core_(var, config, *, options: List[str]):
    cg.add(var.set_name(config[CONF_NAME]))
    cg.add(var.set_disabled_by_default(config[CONF_DISABLED_BY_DEFAULT]))
    if CONF_INTERNAL in config:
        cg.add(var.set_internal(config[CONF_INTERNAL]))

    cg.add(var.traits.set_icon(config[CONF_ICON]))
    cg.add(var.traits.set_options(options))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)


async def register_select(var, config, *, options: List[str]):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_select(var))
    await setup_select_core_(var, config, options=options)


async def new_select(config, *, options: List[str]):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_select(var, config, options=options)
    return var


@coroutine_with_priority(40.0)
async def to_code(config):
    cg.add_define("USE_SELECT")
    cg.add_global(select_ns.using)


@automation.register_action(
    "select.set",
    SelectSetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(Select),
            cv.Required(CONF_OPTION): cv.templatable(cv.string_strict),
        }
    ),
)
async def select_set_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_OPTION], args, str)
    cg.add(var.set_option(template_))
    return var
