import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import mqtt
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_ON_PRESS,
    CONF_TRIGGER_ID,
    CONF_MQTT_ID,
    DEVICE_CLASS_RESTART,
    DEVICE_CLASS_UPDATE,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True

DEVICE_CLASSES = [
    DEVICE_CLASS_RESTART,
    DEVICE_CLASS_UPDATE,
]

button_ns = cg.esphome_ns.namespace("button")
Button = button_ns.class_("Button", cg.EntityBase)
ButtonPtr = Button.operator("ptr")

PressAction = button_ns.class_("PressAction", automation.Action)

ButtonPressTrigger = button_ns.class_(
    "ButtonPressTrigger", automation.Trigger.template()
)

validate_device_class = cv.one_of(*DEVICE_CLASSES, lower=True, space="_")


BUTTON_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA).extend(
    {
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTButtonComponent),
        cv.Optional(CONF_DEVICE_CLASS): validate_device_class,
        cv.Optional(CONF_ON_PRESS): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ButtonPressTrigger),
            }
        ),
    }
)

_UNDEF = object()


def button_schema(
    icon: str = _UNDEF,
    entity_category: str = _UNDEF,
    device_class: str = _UNDEF,
) -> cv.Schema:
    schema = BUTTON_SCHEMA
    if icon is not _UNDEF:
        schema = schema.extend({cv.Optional(CONF_ICON, default=icon): cv.icon})
    if entity_category is not _UNDEF:
        schema = schema.extend(
            {
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=entity_category
                ): cv.entity_category
            }
        )
    if device_class is not _UNDEF:
        schema = schema.extend(
            {
                cv.Optional(
                    CONF_DEVICE_CLASS, default=device_class
                ): validate_device_class
            }
        )
    return schema


async def setup_button_core_(var, config):
    await setup_entity(var, config)

    for conf in config.get(CONF_ON_PRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    if CONF_DEVICE_CLASS in config:
        cg.add(var.set_device_class(config[CONF_DEVICE_CLASS]))

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)


async def register_button(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_button(var))
    await setup_button_core_(var, config)


async def new_button(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_button(var, config)
    return var


BUTTON_PRESS_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Button),
    }
)


@automation.register_action("button.press", PressAction, BUTTON_PRESS_SCHEMA)
async def button_press_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(button_ns.using)
    cg.add_define("USE_BUTTON")
