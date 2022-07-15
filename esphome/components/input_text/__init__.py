from typing import Text
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_ON_VALUE,
    CONF_TRIGGER_ID,
    CONF_MQTT_ID,
    CONF_VALUE,
)

from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True

input_text_ns = cg.esphome_ns.namespace("input_text")
InputText = input_text_ns.class_("InputText", cg.EntityBase)
InputTextPtr = InputText.operator("ptr")

# Triggers
InputTextStateTrigger = input_text_ns.class_(
    "InputTextStateTrigger", automation.Trigger.template(cg.float_)
)

# Actions
InputTextSetAction = input_text_ns.class_("InputTextSetAction", automation.Action)

# Conditions
InputTextMode = input_text_ns.enum("InputTextMode")

INPUT_TEXT_MODES = {
    "AUTO": InputTextMode.INPUT_TEXT_MODE_AUTO,
    "STRING": InputTextMode.INPUT_TEXT_MODE_STRING,
    "PASSWORD": InputTextMode.INPUT_TEXT_MODE_PASSWORD,  # to be implemented for keys, passwords, etc.
}

icon = cv.icon

INPUT_TEXT_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMPONENT_SCHEMA).extend( # of MQTT_COMMAND_COMPONENT_SCHEMA
    {
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTInputTextComponent),
        cv.GenerateID(): cv.declare_id(InputText),
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(InputTextStateTrigger),
            }
        ),
        cv.Optional(CONF_MODE, default="AUTO"): cv.enum(INPUT_TEXT_MODES, upper=True),
    }
)


async def setup_input_text_core_(
    var, config
):
    await setup_entity(var, config)

    cg.add(var.traits.set_mode(config[CONF_MODE]))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)


async def register_input_text(
    var, config
):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_input_text(var))
    await setup_input_text_core_(
        var, config
    )

#
# for copy component => Not yet implemented
# 
#async def new_input_text(
#    config
#):
#    var = cg.new_Pvariable(config[CONF_ID])
#    await register_input_text(
#        var, config
#    )
#    return var



@coroutine_with_priority(40.0)
async def to_code(config):
    cg.add_define("USE_INPUT_TEXT")
    cg.add_global(input_text_ns.using)

OPERATION_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(InputText),
    }
)

@automation.register_action(
    "input_text.set",
    InputTextSetAction,
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_VALUE): cv.templatable(cv.string_strict),
        }
    ),
)
async def input_text_set_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_VALUE], args, cg.std_string)
    cg.add(var.set_value(template_))
    return var
