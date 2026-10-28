from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_ON_LOCK,
    CONF_ON_UNLOCK,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority, entity_helpers
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    mqtt_component_class,
    queue_entity_register,
    setup_entity,
)
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType, SafeExpType

CODEOWNERS = ["@esphome/core"]
DOMAIN = "lock"
IS_PLATFORM_COMPONENT = True

lock_ns = cg.esphome_ns.namespace("lock")
Lock = lock_ns.class_("Lock", cg.EntityBase)
LockPtr = Lock.operator("ptr")
LockCall = lock_ns.class_("LockCall")

LockPublishAction = lock_ns.class_("LockPublishAction", automation.Action)

LockState = lock_ns.enum("LockState")

LOCK_STATES = LockState.options(
    "OPEN",
    "LOCKED",
    "UNLOCKED",
    "JAMMED",
    "OPENING",
    "LOCKING",
    "UNLOCKING",
    prefix="LOCK_STATE_",
)

validate_lock_state = cv.enum(LOCK_STATES, upper=True)

_LOCK_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(entity_helpers.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt_component_class("MQTTLockComponent")
            ),
            cv.Optional(CONF_ON_LOCK): automation.validate_automation({}),
            cv.Optional(CONF_ON_UNLOCK): automation.validate_automation({}),
        }
    )
)


_LOCK_SCHEMA.add_extra(entity_duplicate_validator("lock"))


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


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_LOCK,
        "add_on_state_callback",
        params=[(LockState, "state")],
        forward=[],
        when="state == lock::LOCK_STATE_LOCKED",
    ),
    automation.CallbackAutomation(
        CONF_ON_UNLOCK,
        "add_on_state_callback",
        params=[(LockState, "state")],
        forward=[],
        when="state == lock::LOCK_STATE_UNLOCKED",
    ),
)


@setup_entity("lock")
async def _setup_lock_core(var: MockObj, config: ConfigType) -> None:
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)

    await entity_helpers.setup_entity_integrations(var, config)


async def register_lock(var: MockObj, config: ConfigType) -> None:
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("lock", config)
    CORE.register_platform_component("lock", var)
    await _setup_lock_core(var, config)


async def new_lock(config: ConfigType, *args: SafeExpType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_lock(var, config)
    return var


LOCK_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(CONF_ID): cv.use_id(Lock),
    }
)


for _name, _call in (
    ("lock.unlock", "unlock()"),
    ("lock.lock", "lock()"),
    ("lock.open", "open()"),
):
    automation.register_apply_action(
        _name, LOCK_ACTION_SCHEMA, automation.ApplyCall(_call)
    )


automation.register_apply_condition(
    "lock.is_locked", LOCK_ACTION_SCHEMA, f"state == {LockState.LOCK_STATE_LOCKED}"
)
automation.register_apply_condition(
    "lock.is_unlocked",
    LOCK_ACTION_SCHEMA,
    f"state == {LockState.LOCK_STATE_UNLOCKED}",
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_global(lock_ns.using)
