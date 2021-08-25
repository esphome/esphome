from typing import Optional
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.const import (
    CONF_ABOVE,
    CONF_BELOW,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ICON,
    CONF_ID,
    CONF_INTERNAL,
    CONF_ON_VALUE,
    CONF_ON_VALUE_RANGE,
    CONF_TRIGGER_ID,
    CONF_NAME,
    CONF_MQTT_ID,
    CONF_VALUE,
    ICON_EMPTY,
)
from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True

number_ns = cg.esphome_ns.namespace("number")
Number = number_ns.class_("Number", cg.Nameable)
NumberPtr = Number.operator("ptr")

# Triggers
NumberStateTrigger = number_ns.class_(
    "NumberStateTrigger", automation.Trigger.template(cg.float_)
)
ValueRangeTrigger = number_ns.class_(
    "ValueRangeTrigger", automation.Trigger.template(cg.float_), cg.Component
)

# Actions
NumberSetAction = number_ns.class_("NumberSetAction", automation.Action)

# Conditions
NumberInRangeCondition = number_ns.class_(
    "NumberInRangeCondition", automation.Condition
)

icon = cv.icon


NUMBER_SCHEMA = cv.NAMEABLE_SCHEMA.extend(cv.MQTT_COMPONENT_SCHEMA).extend(
    {
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTNumberComponent),
        cv.GenerateID(): cv.declare_id(Number),
        cv.Optional(CONF_ICON, default=ICON_EMPTY): icon,
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NumberStateTrigger),
            }
        ),
        cv.Optional(CONF_ON_VALUE_RANGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ValueRangeTrigger),
                cv.Optional(CONF_ABOVE): cv.float_,
                cv.Optional(CONF_BELOW): cv.float_,
            },
            cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW),
        ),
    }
)


async def setup_number_core_(
    var, config, *, min_value: float, max_value: float, step: Optional[float]
):
    cg.add(var.set_name(config[CONF_NAME]))
    cg.add(var.set_disabled_by_default(config[CONF_DISABLED_BY_DEFAULT]))
    if CONF_INTERNAL in config:
        cg.add(var.set_internal(config[CONF_INTERNAL]))

    cg.add(var.traits.set_icon(config[CONF_ICON]))
    cg.add(var.traits.set_min_value(min_value))
    cg.add(var.traits.set_max_value(max_value))
    if step is not None:
        cg.add(var.traits.set_step(step))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(float, "x")], conf)
    for conf in config.get(CONF_ON_VALUE_RANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await cg.register_component(trigger, conf)
        if CONF_ABOVE in conf:
            template_ = await cg.templatable(conf[CONF_ABOVE], [(float, "x")], float)
            cg.add(trigger.set_min(template_))
        if CONF_BELOW in conf:
            template_ = await cg.templatable(conf[CONF_BELOW], [(float, "x")], float)
            cg.add(trigger.set_max(template_))
        await automation.build_automation(trigger, [(float, "x")], conf)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)


async def register_number(
    var, config, *, min_value: float, max_value: float, step: Optional[float] = None
):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_number(var))
    await setup_number_core_(
        var, config, min_value=min_value, max_value=max_value, step=step
    )


async def new_number(
    config, *, min_value: float, max_value: float, step: Optional[float] = None
):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_number(
        var, config, min_value=min_value, max_value=max_value, step=step
    )
    return var


NUMBER_IN_RANGE_CONDITION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(Number),
        cv.Optional(CONF_ABOVE): cv.float_,
        cv.Optional(CONF_BELOW): cv.float_,
    },
    cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW),
)


@automation.register_condition(
    "number.in_range", NumberInRangeCondition, NUMBER_IN_RANGE_CONDITION_SCHEMA
)
async def number_in_range_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(condition_id, template_arg, paren)

    if CONF_ABOVE in config:
        cg.add(var.set_min(config[CONF_ABOVE]))
    if CONF_BELOW in config:
        cg.add(var.set_max(config[CONF_BELOW]))

    return var


@coroutine_with_priority(40.0)
async def to_code(config):
    cg.add_define("USE_NUMBER")
    cg.add_global(number_ns.using)


@automation.register_action(
    "number.set",
    NumberSetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(Number),
            cv.Required(CONF_VALUE): cv.templatable(cv.float_),
        }
    ),
)
async def number_set_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_VALUE], args, float)
    cg.add(var.set_value(template_))
    return var
