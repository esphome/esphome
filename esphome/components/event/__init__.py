from esphome import automation
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_EVENT_TYPE,
    CONF_ICON,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_ON_EVENT,
    CONF_WEB_SERVER,
    DEVICE_CLASS_BUTTON,
    DEVICE_CLASS_DOORBELL,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_MOTION,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    queue_entity_register,
    setup_device_class,
    setup_entity,
)
from esphome.cpp_generator import MockObjClass

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

validate_device_class = cv.one_of(*DEVICE_CLASSES, lower=True, space="_")

_EVENT_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTEventComponent),
            cv.GenerateID(): cv.declare_id(Event),
            cv.Optional(CONF_DEVICE_CLASS): validate_device_class,
            cv.Optional(CONF_ON_EVENT): automation.validate_automation({}),
        }
    )
)


_EVENT_SCHEMA.add_extra(entity_duplicate_validator("event"))


def event_schema(
    class_: MockObjClass = cv.UNDEFINED,
    *,
    icon: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
    device_class: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {}

    if class_ is not cv.UNDEFINED:
        schema[cv.GenerateID()] = cv.declare_id(class_)

    for key, default, validator in [
        (CONF_ICON, icon, cv.icon),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_DEVICE_CLASS, device_class, validate_device_class),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _EVENT_SCHEMA.extend(schema)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_EVENT, "add_on_event_callback", [(cg.StringRef, "event_type")]
    ),
)


@setup_entity("event")
async def setup_event_core_(var, config, *, event_types: list[str]):
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)

    cg.add(var.set_event_types(event_types))

    setup_device_class(config)

    if mqtt_id := config.get(CONF_MQTT_ID):
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)


async def register_event(var, config, *, event_types: list[str]):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("event", config)
    CORE.register_platform_component("event", var)
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


@automation.register_action(
    "event.trigger", TriggerEventAction, TRIGGER_EVENT_SCHEMA, synchronous=True
)
async def event_fire_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    templ = await cg.templatable(config[CONF_EVENT_TYPE], args, cg.std_string)
    cg.add(var.set_event_type(templ))
    return var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(event_ns.using)
