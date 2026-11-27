from collections.abc import Callable

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_DIRECTION_COMMAND_TOPIC,
    CONF_DIRECTION_STATE_TOPIC,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_OFF_SPEED_CYCLE,
    CONF_ON_DIRECTION_SET,
    CONF_ON_OSCILLATING_SET,
    CONF_ON_PRESET_SET,
    CONF_ON_SPEED_SET,
    CONF_ON_STATE,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_OSCILLATING,
    CONF_OSCILLATION_COMMAND_TOPIC,
    CONF_OSCILLATION_STATE_TOPIC,
    CONF_RESTORE_MODE,
    CONF_SPEED,
    CONF_SPEED_COMMAND_TOPIC,
    CONF_SPEED_LEVEL_COMMAND_TOPIC,
    CONF_SPEED_LEVEL_STATE_TOPIC,
    CONF_SPEED_STATE_TOPIC,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority, entity_helpers
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    mqtt_component_class,
    queue_entity_register,
    setup_entity,
)
from esphome.cpp_generator import (
    BinOpExpression,
    CallExpression,
    Expression,
    MockObj,
    StaticCastExpression,
    TemplateArgsType,
    UnaryOpExpression,
    VariableDeclarationExpression,
)
from esphome.types import ConfigType

DOMAIN = "fan"
IS_PLATFORM_COMPONENT = True

fan_ns = cg.esphome_ns.namespace("fan")
Fan = fan_ns.class_("Fan", cg.EntityBase)

FanDirection = fan_ns.enum("FanDirection", is_class=True)
FAN_DIRECTION_ENUM = FanDirection.options("FORWARD", "REVERSE")

FanRestoreMode = fan_ns.enum("FanRestoreMode", is_class=True)
RESTORE_MODES = FanRestoreMode.options(
    "NO_RESTORE",
    "ALWAYS_OFF",
    "ALWAYS_ON",
    "RESTORE_DEFAULT_OFF",
    "RESTORE_DEFAULT_ON",
    "RESTORE_INVERTED_DEFAULT_OFF",
    "RESTORE_INVERTED_DEFAULT_ON",
)

_FAN_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(entity_helpers.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_RESTORE_MODE, default="ALWAYS_OFF"): cv.enum(
                RESTORE_MODES, upper=True, space="_"
            ),
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt_component_class("MQTTFanComponent")
            ),
            cv.Optional(CONF_DIRECTION_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_DIRECTION_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.subscribe_topic
            ),
            cv.Optional(CONF_OSCILLATION_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_OSCILLATION_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.subscribe_topic
            ),
            cv.Optional(CONF_SPEED_LEVEL_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_SPEED_LEVEL_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.subscribe_topic
            ),
            cv.Optional(CONF_SPEED_STATE_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.publish_topic
            ),
            cv.Optional(CONF_SPEED_COMMAND_TOPIC): cv.All(
                cv.requires_component("mqtt"), cv.subscribe_topic
            ),
            cv.Optional(CONF_ON_STATE): automation.validate_automation(),
            cv.Optional(CONF_ON_TURN_ON): automation.validate_automation(),
            cv.Optional(CONF_ON_TURN_OFF): automation.validate_automation(),
            cv.Optional(CONF_ON_DIRECTION_SET): automation.validate_automation(),
            cv.Optional(CONF_ON_OSCILLATING_SET): automation.validate_automation(),
            cv.Optional(CONF_ON_SPEED_SET): automation.validate_automation(),
            cv.Optional(CONF_ON_PRESET_SET): automation.validate_automation(),
        }
    )
)


_FAN_SCHEMA.add_extra(entity_duplicate_validator("fan"))


def fan_schema(
    class_: cg.Pvariable,
    *,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
    default_restore_mode: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {
        cv.GenerateID(): cv.declare_id(class_),
    }

    for key, default, validator in [
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_ICON, icon, cv.icon),
        (
            CONF_RESTORE_MODE,
            default_restore_mode,
            cv.enum(RESTORE_MODES, upper=True, space="_"),
        ),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _FAN_SCHEMA.extend(schema)


_PRESET_MODES_SCHEMA = cv.All(
    cv.ensure_list(cv.string_strict),
    cv.Length(min=1),
)


# speed_count_ is a uint16_t in FanTraits
validate_speed_count = cv.int_range(min=1, max=65535)


def validate_preset_modes(value):
    # Check against defined schema
    value = _PRESET_MODES_SCHEMA(value)

    # Ensure preset names are unique
    errors = []
    presets = set()
    for i, preset in enumerate(value):
        # If name does not exist yet add it
        if preset not in presets:
            presets.add(preset)
            continue

        # Otherwise it's an error
        errors.append(
            cv.Invalid(
                f"Found duplicate preset name '{preset}'. Presets must have unique names.",
                [i],
            )
        )

    if errors:
        raise cv.MultipleInvalid(errors)

    return value


def set_preset_modes(var: MockObj, preset_modes: list[str]) -> None:
    """Point the fan at a shared flash table of preset modes."""
    if preset_modes:
        table = cg.shared_progmem_array(
            "fan_preset_modes", cg.const_char_ptr, preset_modes
        )
        cg.add(var.set_supported_preset_modes_static(table, len(preset_modes)))


def _changed(last: MockObj, value: MockObj) -> Expression:
    return fan_ns.update_changed(last, value)


def _turned_on(last: MockObj, value: MockObj) -> Expression:
    return BinOpExpression(_changed(last, value), "&&", value)


def _turned_off(last: MockObj, value: MockObj) -> Expression:
    return BinOpExpression(_changed(last, value), "&&", UnaryOpExpression("!", value))


# key, automation args, watched fan value, type of the last value kept, filter that also updates it.
# The last values start at zero, as the fan's own values do.
_CHANGE_TRIGGERS: tuple[
    tuple[
        str,
        TemplateArgsType,
        Callable[[MockObj], MockObj],
        MockObj,
        Callable[[MockObj, MockObj], Expression],
    ],
    ...,
] = (
    (CONF_ON_TURN_ON, [], lambda fan: fan.state, cg.bool_, _turned_on),
    (CONF_ON_TURN_OFF, [], lambda fan: fan.state, cg.bool_, _turned_off),
    (
        CONF_ON_DIRECTION_SET,
        [(FanDirection, "x")],
        lambda fan: fan.direction,
        FanDirection,
        _changed,
    ),
    (
        CONF_ON_OSCILLATING_SET,
        [(cg.bool_, "x")],
        lambda fan: fan.oscillating,
        cg.bool_,
        _changed,
    ),
    (CONF_ON_SPEED_SET, [(cg.int_, "x")], lambda fan: fan.speed, cg.int_, _changed),
    (
        CONF_ON_PRESET_SET,
        [(cg.StringRef, "x")],
        lambda fan: fan.get_preset_mode(),
        cg.const_char_ptr,
        fan_ns.update_preset_changed,
    ),
)


async def _build_change_triggers(var: MockObj, config: ConfigType) -> None:
    """Register one state callback that runs the change triggers; each keeps its last value in a global."""
    fan = automation.parent_ref(var)
    callbacks: list[MockObj] = []
    for conf_key, args, watched, last_type, check in _CHANGE_TRIGGERS:
        value = watched(fan)
        for index, conf in enumerate(config.get(conf_key, [])):
            last_id = cg.unique_global_id(
                f"esphome__{var}__{conf_key}_{index}__last", last_type
            )
            last = cg.MockObj(last_id)
            CORE.register_variable(last_id, last)
            cg.add_global(
                VariableDeclarationExpression(last_type, "", last, static=True)
            )
            # A copy, as the actions take x by reference and may change the fan
            forward = [StaticCastExpression(type_, value) for type_, _ in args]
            callbacks.append(
                await automation.build_trigger_callback(
                    args,
                    conf,
                    params=[],
                    forward=forward,
                    when=str(check(last, value)),
                )
            )
    if len(callbacks) > 1:
        callbacks = [
            cg.shared_static_function(
                f"esphome__{var}__change_fn",
                cg.void,
                [],
                [cg.statement(CallExpression(callback)) for callback in callbacks],
                share=False,
            )
        ]
    for callback in callbacks:
        cg.add(var.add_on_state_callback(callback))


@setup_entity("fan")
async def setup_fan_core_(var, config):
    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    mqtt_ = await entity_helpers.setup_entity_integrations(var, config)
    if mqtt_ is not None:
        if (
            direction_state_topic := config.get(CONF_DIRECTION_STATE_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_direction_state_topic(direction_state_topic))
        if (
            direction_command_topic := config.get(CONF_DIRECTION_COMMAND_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_direction_command_topic(direction_command_topic))
        if (
            oscillation_state_topic := config.get(CONF_OSCILLATION_STATE_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_oscillation_state_topic(oscillation_state_topic))
        if (
            oscillation_command_topic := config.get(CONF_OSCILLATION_COMMAND_TOPIC)
        ) is not None:
            cg.add(
                mqtt_.set_custom_oscillation_command_topic(oscillation_command_topic)
            )
        if (
            speed_level_state_topic := config.get(CONF_SPEED_LEVEL_STATE_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_speed_level_state_topic(speed_level_state_topic))
        if (
            speed_level_command_topic := config.get(CONF_SPEED_LEVEL_COMMAND_TOPIC)
        ) is not None:
            cg.add(
                mqtt_.set_custom_speed_level_command_topic(speed_level_command_topic)
            )
        if (speed_state_topic := config.get(CONF_SPEED_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_speed_state_topic(speed_state_topic))
        if (speed_command_topic := config.get(CONF_SPEED_COMMAND_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_speed_command_topic(speed_command_topic))

    for conf in config.get(CONF_ON_STATE, []):
        await automation.build_parent_callback_automation(
            var, "add_on_state_callback", (Fan.operator("ptr"), "x"), conf
        )
    await _build_change_triggers(var, config)


async def register_fan(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("fan", config)
    CORE.register_platform_component("fan", var)
    await setup_fan_core_(var, config)


async def new_fan(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_fan(var, config)
    return var


FAN_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Fan),
    }
)


automation.register_apply_action("fan.toggle", FAN_ACTION_SCHEMA, call="toggle")
automation.register_apply_action("fan.turn_off", FAN_ACTION_SCHEMA, call="turn_off")

automation.register_apply_action(
    "fan.turn_on",
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Fan),
            cv.Optional(CONF_OSCILLATING): cv.templatable(cv.boolean),
            cv.Optional(CONF_SPEED): cv.templatable(cv.int_range(1)),
            cv.Optional(CONF_DIRECTION): cv.templatable(
                cv.enum(FAN_DIRECTION_ENUM, upper=True)
            ),
        }
    ),
    automation.ApplyField(CONF_OSCILLATING, "set_oscillating", cg.bool_),
    automation.ApplyField(CONF_SPEED, "set_speed", cg.int_),
    automation.ApplyField(CONF_DIRECTION, "set_direction", FanDirection),
    call="turn_on",
)


@automation.register_action(
    "fan.cycle_speed",
    automation.ApplyAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Fan),
            cv.Optional(CONF_OFF_SPEED_CYCLE, default=True): cv.boolean,
        }
    ),
    synchronous=True,
)
async def fan_cycle_speed_to_code(config, action_id, template_arg, args):
    paren = automation.parent_ref(await cg.get_variable(config[CONF_ID]))
    text = str(cg.statement(fan_ns.cycle_speed(paren, config[CONF_OFF_SPEED_CYCLE])))
    return automation.apply_statements_action(action_id, template_arg, args, [text])


FAN_CONDITION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Fan),
    }
)

automation.register_apply_condition("fan.is_on", FAN_CONDITION_SCHEMA, "state")
automation.register_apply_condition(
    "fan.is_off", FAN_CONDITION_SCHEMA, "state == false"
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(fan_ns.using)
