from esphome import automation
import esphome.codegen as cg
from esphome.components import mqtt
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_EVENT_TYPE,
    CONF_ICON,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_ON_EVENT,
    CONF_TRIGGER_ID,
    DEVICE_CLASS_BUTTON,
    DEVICE_CLASS_DOORBELL,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_MOTION,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_generator import MockObjClass
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@nohat"]
IS_PLATFORM_COMPONENT = True

DEVICE_CLASSES = [
    DEVICE_CLASS_BUTTON,
    DEVICE_CLASS_DOORBELL,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_MOTION,
]

event_ns = cg.esphome_ns.namespace("event")
Event = event_ns.class_("Event", cg.EntityBase)
EventPtr = Event.operator("ptr")

TriggerEventAction = event_ns.class_("TriggerEventAction", automation.Action)

EventTrigger = event_ns.class_("EventTrigger", automation.Trigger.template())

validate_device_class = cv.one_of(*DEVICE_CLASSES, lower=True, space="_")

EVENT_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMPONENT_SCHEMA).extend(
    {
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTEventComponent),
        cv.GenerateID(): cv.declare_id(Event),
        cv.Optional(CONF_DEVICE_CLASS): validate_device_class,
        cv.Optional(CONF_ON_EVENT): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(EventTrigger),
            }
        ),
    }
)

_UNDEF = object()


def event_schema(
    class_: MockObjClass = _UNDEF,
    *,
    icon: str = _UNDEF,
    entity_category: str = _UNDEF,
    device_class: str = _UNDEF,
) -> cv.Schema:
    schema = {}

    if class_ is not _UNDEF:
        schema[cv.GenerateID()] = cv.declare_id(class_)

    for key, default, validator in [
        (CONF_ICON, icon, cv.icon),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_DEVICE_CLASS, device_class, validate_device_class),
    ]:
        if default is not _UNDEF:
            schema[cv.Optional(key, default=default)] = validator

    return EVENT_SCHEMA.extend(schema)


async def setup_event_core_(var, config, *, event_types: list[str]):
    await setup_entity(var, config)

    for conf in config.get(CONF_ON_EVENT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "event_type")], conf
        )

    cg.add(var.set_event_types(event_types))

    if (device_class := config.get(CONF_DEVICE_CLASS)) is not None:
        cg.add(var.set_device_class(device_class))

    if mqtt_id := config.get(CONF_MQTT_ID):
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)


async def register_event(var, config, *, event_types: list[str]):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_event(var))
    await setup_event_core_(var, config, event_types=event_types)


async def new_event(config, *, event_types: list[str]):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_event(var, config, event_types=event_types)
    return var


TRIGGER_EVENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Event),
        cv.Required(CONF_EVENT_TYPE): cv.templatable(cv.string_strict),
    }
)


@automation.register_action("event.trigger", TriggerEventAction, TRIGGER_EVENT_SCHEMA)
async def event_fire_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    templ = await cg.templatable(config[CONF_EVENT_TYPE], args, cg.std_string)
    cg.add(var.set_event_type(templ))
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_EVENT")
    cg.add_global(event_ns.using)
