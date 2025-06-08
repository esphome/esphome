from esphome import automation
from esphome.automation import Condition, maybe_simple_id
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_ON_LOCK,
    CONF_ON_UNLOCK,
    CONF_TRIGGER_ID,
    CONF_WEB_SERVER,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_generator import MockObjClass
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True

lock_ns = cg.esphome_ns.namespace("lock")
Lock = lock_ns.class_("Lock", cg.EntityBase)
LockPtr = Lock.operator("ptr")
LockCall = lock_ns.class_("LockCall")

UnlockAction = lock_ns.class_("UnlockAction", automation.Action)
LockAction = lock_ns.class_("LockAction", automation.Action)
OpenAction = lock_ns.class_("OpenAction", automation.Action)
LockPublishAction = lock_ns.class_("LockPublishAction", automation.Action)

LockCondition = lock_ns.class_("LockCondition", Condition)
LockLockTrigger = lock_ns.class_("LockLockTrigger", automation.Trigger.template())
LockUnlockTrigger = lock_ns.class_("LockUnlockTrigger", automation.Trigger.template())

LockState = lock_ns.enum("LockState")

LOCK_STATES = {
    "LOCKED": LockState.LOCK_STATE_LOCKED,
    "UNLOCKED": LockState.LOCK_STATE_UNLOCKED,
    "JAMMED": LockState.LOCK_STATE_JAMMED,
    "LOCKING": LockState.LOCK_STATE_LOCKING,
    "UNLOCKING": LockState.LOCK_STATE_UNLOCKING,
}

validate_lock_state = cv.enum(LOCK_STATES, upper=True)

_LOCK_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTLockComponent),
            cv.Optional(CONF_ON_LOCK): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LockLockTrigger),
                }
            ),
            cv.Optional(CONF_ON_UNLOCK): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LockUnlockTrigger),
                }
            ),
        }
    )
)


def lock_schema(
    class_: MockObjClass = cv.UNDEFINED,
    *,
    icon: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {}

    if class_ is not cv.UNDEFINED:
        schema[cv.GenerateID()] = cv.declare_id(class_)

    for key, default, validator in [
        (CONF_ICON, icon, cv.icon),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _LOCK_SCHEMA.extend(schema)


# Remove before 2025.11.0
LOCK_SCHEMA = lock_schema()
LOCK_SCHEMA.add_extra(cv.deprecated_schema_constant("lock"))


async def _setup_lock_core(var, config):
    await setup_entity(var, config)

    for conf in config.get(CONF_ON_LOCK, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_UNLOCK, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    if mqtt_id := config.get(CONF_MQTT_ID):
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)


async def register_lock(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_lock(var))
    await _setup_lock_core(var, config)


async def new_lock(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_lock(var, config)
    return var


LOCK_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(CONF_ID): cv.use_id(Lock),
    }
)


@automation.register_action("lock.unlock", UnlockAction, LOCK_ACTION_SCHEMA)
@automation.register_action("lock.lock", LockAction, LOCK_ACTION_SCHEMA)
@automation.register_action("lock.open", OpenAction, LOCK_ACTION_SCHEMA)
async def lock_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_condition("lock.is_locked", LockCondition, LOCK_ACTION_SCHEMA)
async def lock_is_on_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, True)


@automation.register_condition("lock.is_unlocked", LockCondition, LOCK_ACTION_SCHEMA)
async def lock_is_off_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, False)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(lock_ns.using)
    cg.add_define("USE_LOCK")
