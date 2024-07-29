from typing import Optional
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt, web_server
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_ON_VALUE,
    CONF_TRIGGER_ID,
    CONF_MQTT_ID,
    CONF_WEB_SERVER_ID,
    CONF_VALUE,
)

from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@mauritskorse"]
IS_PLATFORM_COMPONENT = True

text_ns = cg.esphome_ns.namespace("text")
Text = text_ns.class_("Text", cg.EntityBase)
TextPtr = Text.operator("ptr")

# Triggers
TextStateTrigger = text_ns.class_(
    "TextStateTrigger", automation.Trigger.template(cg.std_string)
)

# Actions
TextSetAction = text_ns.class_("TextSetAction", automation.Action)

# Conditions
TextMode = text_ns.enum("TextMode")

TEXT_MODES = {
    "TEXT": TextMode.TEXT_MODE_TEXT,
    "PASSWORD": TextMode.TEXT_MODE_PASSWORD,  # to be implemented for keys, passwords, etc.
}

TEXT_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTTextComponent),
            cv.GenerateID(): cv.declare_id(Text),
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TextStateTrigger),
                }
            ),
            cv.Required(CONF_MODE): cv.enum(TEXT_MODES, upper=True),
        }
    )
)


async def setup_text_core_(
    var,
    config,
    *,
    min_length: Optional[int],
    max_length: Optional[int],
    pattern: Optional[str],
):
    await setup_entity(var, config)

    cg.add(var.traits.set_min_length(min_length))
    cg.add(var.traits.set_max_length(max_length))
    if pattern is not None:
        cg.add(var.traits.set_pattern(pattern))

    cg.add(var.traits.set_mode(config[CONF_MODE]))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if (webserver_id := config.get(CONF_WEB_SERVER_ID)) is not None:
        web_server_ = await cg.get_variable(webserver_id)
        web_server.add_entity_to_sorting_list(web_server_, var, config)


async def register_text(
    var,
    config,
    *,
    min_length: Optional[int] = 0,
    max_length: Optional[int] = 255,
    pattern: Optional[str] = None,
):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_text(var))
    await setup_text_core_(
        var, config, min_length=min_length, max_length=max_length, pattern=pattern
    )


async def new_text(
    config,
    *,
    min_length: Optional[int] = 0,
    max_length: Optional[int] = 255,
    pattern: Optional[str] = None,
):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_text(
        var, config, min_length=min_length, max_length=max_length, pattern=pattern
    )
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_TEXT")
    cg.add_global(text_ns.using)


OPERATION_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Text),
    }
)


@automation.register_action(
    "text.set",
    TextSetAction,
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_VALUE): cv.templatable(cv.string_strict),
        }
    ),
)
async def text_set_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_VALUE], args, cg.std_string)
    cg.add(var.set_value(template_))
    return var
