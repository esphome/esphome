from esphome import automation, preferences
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import mqtt, web_server, zigbee
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_INITIAL_STATE,
    CONF_INVERTED,
    CONF_MQTT_ID,
    CONF_ON_STATE,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_RESTORE_MODE,
    CONF_RESTORE_STATE,
    CONF_STATE,
    CONF_STORAGE,
    CONF_WEB_SERVER,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_OUTLET,
    DEVICE_CLASS_SWITCH,
)
from esphome.core import CORE, ID, CoroPriority, coroutine_with_priority
from esphome.core.entity_helpers import (
    SubEntities,
    entity_duplicate_validator,
    queue_entity_register,
    setup_device_class,
    setup_entity,
)
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DOMAIN = "switch"
IS_PLATFORM_COMPONENT = True
DEVICE_CLASSES = [
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_OUTLET,
    DEVICE_CLASS_SWITCH,
]

switch_ns = cg.esphome_ns.namespace("switch_")
Switch = switch_ns.class_("Switch", cg.EntityBase)
SwitchPtr = Switch.operator("ptr")

SwitchRestoreMode = switch_ns.enum("SwitchRestoreMode")

RESTORE_MODES = {
    "RESTORE_DEFAULT_OFF": SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_OFF,
    "RESTORE_DEFAULT_ON": SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_ON,
    "ALWAYS_OFF": SwitchRestoreMode.SWITCH_ALWAYS_OFF,
    "ALWAYS_ON": SwitchRestoreMode.SWITCH_ALWAYS_ON,
    "RESTORE_INVERTED_DEFAULT_OFF": SwitchRestoreMode.SWITCH_RESTORE_INVERTED_DEFAULT_OFF,
    "RESTORE_INVERTED_DEFAULT_ON": SwitchRestoreMode.SWITCH_RESTORE_INVERTED_DEFAULT_ON,
    "DISABLED": SwitchRestoreMode.SWITCH_RESTORE_DISABLED,
}


validate_device_class = cv.one_of(*DEVICE_CLASSES, lower=True)


RESTORE_STATE_KEEP = "KEEP"
RESTORE_STATE_INVERT = "INVERT"
RESTORE_STATE_INITIAL = "INITIAL"
RESTORE_STATE_ALL = "ALL"
RESTORE_STATE_NONE = "NONE"
_RESTORE_STATE_STATES = (
    RESTORE_STATE_KEEP,
    RESTORE_STATE_INVERT,
    RESTORE_STATE_INITIAL,
)


def _validate_restore_state_storage(config: ConfigType) -> ConfigType:
    if CONF_STORAGE in config and config[CONF_STATE] == RESTORE_STATE_INITIAL:
        raise cv.Invalid(
            "'storage' only applies when a saved state is restored (KEEP or INVERT)",
            path=[CONF_STORAGE],
        )
    return config


_RESTORE_STATE_FIELDS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_STATE, default=RESTORE_STATE_KEEP): cv.one_of(
            *_RESTORE_STATE_STATES, upper=True
        ),
        # No default: when unset, the existing restore_mode storage applies (on ESP8266,
        # restore_from_flash decides), as with wifi
        cv.Optional(CONF_STORAGE): preferences.validate_storage,
    }
)
_RESTORE_STATE_FIELDS_SCHEMA.add_extra(_validate_restore_state_storage)


@schema_extractor("schema")
def RESTORE_STATE_SCHEMA(value):
    """A mapping of `state` and `storage`, or a shorthand: a state, `all` (KEEP) or `none`."""
    if value == SCHEMA_EXTRACT:
        return _RESTORE_STATE_FIELDS_SCHEMA
    if isinstance(value, str):
        upper = value.strip().upper()
        if upper == RESTORE_STATE_ALL:
            value = {}
        elif upper == RESTORE_STATE_NONE:
            value = {CONF_STATE: RESTORE_STATE_INITIAL}
        else:
            value = {CONF_STATE: upper}
    return _RESTORE_STATE_FIELDS_SCHEMA(value)


def _validate_initial_state(config: ConfigType) -> ConfigType:
    if CONF_INITIAL_STATE in config and CONF_RESTORE_MODE in config:
        raise cv.Invalid(
            "'initial_state' can't be combined with 'restore_mode', which already sets "
            "the boot state; use 'restore_state' instead",
            path=[CONF_INITIAL_STATE],
        )
    return config


# (restore_state state, initial_state) -> restore mode
_RESTORE_MODE_FOR_STATE = {
    (RESTORE_STATE_KEEP, False): "RESTORE_DEFAULT_OFF",
    (RESTORE_STATE_KEEP, True): "RESTORE_DEFAULT_ON",
    (RESTORE_STATE_INVERT, False): "RESTORE_INVERTED_DEFAULT_OFF",
    (RESTORE_STATE_INVERT, True): "RESTORE_INVERTED_DEFAULT_ON",
    (RESTORE_STATE_INITIAL, False): "ALWAYS_OFF",
    (RESTORE_STATE_INITIAL, True): "ALWAYS_ON",
}
_STATE_FOR_RESTORE_MODE = {
    mode: state for (state, _), mode in _RESTORE_MODE_FOR_STATE.items()
}


def _apply_default_restore_mode(default_restore_mode: str):
    # cv.Exclusive has no default, so apply it here when no restore option is configured.
    # With only initial_state set, a restoring default keeps restoring (as light does) and
    # initial_state supplies the state used when nothing is saved.
    default = cv.enum(RESTORE_MODES, upper=True, space="_")(default_restore_mode)
    state = _STATE_FOR_RESTORE_MODE.get(str(default), RESTORE_STATE_INITIAL)

    def validator(config: ConfigType) -> ConfigType:
        if CONF_RESTORE_MODE in config or CONF_RESTORE_STATE in config:
            return config
        if CONF_INITIAL_STATE in config:
            config[CONF_RESTORE_STATE] = {CONF_STATE: state}
        else:
            config[CONF_RESTORE_MODE] = default
        return config

    return validator


def _resolve_restore_mode(config: ConfigType) -> MockObj | str:
    """Translate `initial_state` / `restore_state` into one of the RESTORE_MODES."""
    if (restore_mode := config.get(CONF_RESTORE_MODE)) is not None:
        return restore_mode
    state = config.get(CONF_RESTORE_STATE, {}).get(CONF_STATE, RESTORE_STATE_INITIAL)
    return RESTORE_MODES[
        _RESTORE_MODE_FOR_STATE[state, config.get(CONF_INITIAL_STATE, False)]
    ]


_SWITCH_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(zigbee.SWITCH_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTSwitchComponent),
            cv.Optional(CONF_INVERTED): cv.boolean,
            cv.Exclusive(CONF_RESTORE_MODE, "restore"): cv.enum(
                RESTORE_MODES, upper=True, space="_"
            ),
            cv.Exclusive(CONF_RESTORE_STATE, "restore"): RESTORE_STATE_SCHEMA,
            cv.Optional(CONF_INITIAL_STATE): cv.boolean,
            cv.Optional(CONF_ON_STATE): automation.validate_automation({}),
            cv.Optional(CONF_ON_TURN_ON): automation.validate_automation({}),
            cv.Optional(CONF_ON_TURN_OFF): automation.validate_automation({}),
            cv.Optional(
                CONF_DEVICE_CLASS, visibility=cv.Visibility.ADVANCED
            ): validate_device_class,
        }
    )
)


_SWITCH_SCHEMA.add_extra(entity_duplicate_validator("switch"))
_SWITCH_SCHEMA.add_extra(zigbee.validate_switch)
_SWITCH_SCHEMA.add_extra(_validate_initial_state)


def switch_schema(
    class_: MockObjClass,
    *,
    block_inverted: bool = False,
    default_restore_mode: str = cv.UNDEFINED,
    device_class: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
):
    schema = {cv.GenerateID(): cv.declare_id(class_)}

    for key, default, validator in [
        (CONF_DEVICE_CLASS, device_class, validate_device_class),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_ICON, icon, cv.icon),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    if block_inverted:
        schema[cv.Optional(CONF_INVERTED)] = cv.invalid(
            "Inverted is not supported for this platform!"
        )

    result = _SWITCH_SCHEMA.extend(schema)
    result.add_extra(
        _apply_default_restore_mode(
            "ALWAYS_OFF"
            if default_restore_mode is cv.UNDEFINED
            else default_restore_mode
        )
    )
    return result


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_STATE, "add_on_state_callback", [(bool, "x")]
    ),
    automation.CallbackAutomation(
        CONF_ON_TURN_ON,
        "add_on_state_callback",
        forwarder=automation.TriggerOnTrueForwarder,
    ),
    automation.CallbackAutomation(
        CONF_ON_TURN_OFF,
        "add_on_state_callback",
        forwarder=automation.TriggerOnFalseForwarder,
    ),
)


@coroutine_with_priority(CoroPriority.AUTOMATION)
async def _build_switch_automations(var, config):
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


@setup_entity("switch")
async def setup_switch_core_(var, config):
    if (inverted := config.get(CONF_INVERTED)) is not None:
        cg.add(var.set_inverted(inverted))

    CORE.add_job(_build_switch_automations, var, config)

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)

    setup_device_class(config)

    cg.add(var.set_restore_mode(_resolve_restore_mode(config)))
    if (storage := config.get(CONF_RESTORE_STATE, {}).get(CONF_STORAGE)) is not None:
        cg.add_define("USE_SWITCH_STORAGE")
        cg.add(var.set_restore_in_flash(preferences.is_in_flash(storage)))
    await zigbee.setup_switch(var, config)


async def register_switch(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("switch", config)
    CORE.register_platform_component("switch", var)
    await setup_switch_core_(var, config)


async def new_switch(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_switch(var, config)
    return var


def sub_switches(
    config: ConfigType, *, parent: MockObj | ID | None = None
) -> SubEntities:
    """Return a SubEntities bound to new_switch."""
    return SubEntities(new_switch, config, parent)


SWITCH_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Switch),
    }
)
SWITCH_CONTROL_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Switch),
        cv.Required(CONF_STATE): cv.templatable(cv.boolean),
    }
)


automation.register_apply_action(
    "switch.control",
    SWITCH_CONTROL_ACTION_SCHEMA,
    automation.ApplyField(CONF_STATE, "control", cg.bool_),
)
for _name, _call in (
    ("switch.toggle", "toggle()"),
    ("switch.turn_off", "turn_off()"),
    ("switch.turn_on", "turn_on()"),
):
    automation.register_apply_action(
        _name, SWITCH_ACTION_SCHEMA, automation.ApplyCall(_call)
    )

automation.register_apply_condition("switch.is_on", SWITCH_ACTION_SCHEMA, "state")
automation.register_apply_condition(
    "switch.is_off", SWITCH_ACTION_SCHEMA, "state == false"
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(switch_ns.using)
