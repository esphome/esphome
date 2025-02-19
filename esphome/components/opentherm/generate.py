from collections.abc import Awaitable
from typing import Any, Callable, Optional

import esphome.codegen as cg
from esphome.const import CONF_ID

from . import const
from .schema import SettingSchema, TSchema

opentherm_ns = cg.esphome_ns.namespace("opentherm")
OpenthermHub = opentherm_ns.class_("OpenthermHub", cg.Component)
OpenthermData = opentherm_ns.class_("OpenthermData")


def define_has_component(component_type: str, keys: list[str]) -> None:
    cg.add_define(
        f"OPENTHERM_{component_type.upper()}_LIST(F, sep)",
        cg.RawExpression(
            " sep ".join(map(lambda key: f"F({key}_{component_type.lower()})", keys))
        ),
    )
    for key in keys:
        cg.add_define(f"OPENTHERM_HAS_{component_type.upper()}_{key}")


# We need a separate set of macros for settings because there are different backing field types we need to take
# into account
def define_has_settings(keys: list[str], schemas: dict[str, SettingSchema]) -> None:
    cg.add_define(
        "OPENTHERM_SETTING_LIST(F, sep)",
        cg.RawExpression(
            " sep ".join(
                map(
                    lambda key: f"F({schemas[key].backing_type}, {key}_setting, {schemas[key].default_value})",
                    keys,
                )
            )
        ),
    )
    for key in keys:
        cg.add_define(f"OPENTHERM_HAS_SETTING_{key}")


def define_message_handler(
    component_type: str, keys: list[str], schemas: dict[str, TSchema]
) -> None:
    # The macros defined here should be able to generate things like this:
    # // Parsing a message and publishing to sensors
    # case MessageId::Message:
    #     // Can have multiple sensors here, for example for a Status message with multiple flags
    #     this->thing_binary_sensor->publish_state(parse_flag8_lb_0(response));
    #     this->other_binary_sensor->publish_state(parse_flag8_lb_1(response));
    #     break;
    # // Building a message for a write request
    # case MessageId::Message: {
    #     unsigned int data = 0;
    #     data = write_flag8_lb_0(some_input_switch->state, data); // Where input_sensor can also be a number/output/switch
    #     data = write_u8_hb(some_number->state, data);
    #     return opentherm_->build_request_(MessageType::WriteData, MessageId::Message, data);
    # }

    messages: dict[str, list[tuple[str, str]]] = {}
    for key in keys:
        msg = schemas[key].message
        if msg not in messages:
            messages[msg] = []
        messages[msg].append((key, schemas[key].message_data))

    cg.add_define(
        f"OPENTHERM_{component_type.upper()}_MESSAGE_HANDLERS(MESSAGE, ENTITY, entity_sep, postscript, msg_sep)",
        cg.RawExpression(
            " msg_sep ".join(
                [
                    f"MESSAGE({msg}) "
                    + " entity_sep ".join(
                        [
                            f"ENTITY({key}_{component_type.lower()}, {msg_data})"
                            for key, msg_data in keys
                        ]
                    )
                    + " postscript"
                    for msg, keys in messages.items()
                ]
            )
        ),
    )


def define_readers(component_type: str, keys: list[str]) -> None:
    for key in keys:
        cg.add_define(
            f"OPENTHERM_READ_{key}",
            cg.RawExpression(f"this->{key}_{component_type.lower()}->state"),
        )


def define_setting_readers(component_type: str, keys: list[str]) -> None:
    for key in keys:
        cg.add_define(
            f"OPENTHERM_READ_{key}",
            cg.RawExpression(f"this->{key}_{component_type.lower()}"),
        )


def add_messages(hub: cg.MockObj, keys: list[str], schemas: dict[str, TSchema]):
    messages: dict[str, tuple[bool, Optional[int]]] = {}
    for key in keys:
        messages[schemas[key].message] = (
            schemas[key].keep_updated,
            schemas[key].order if hasattr(schemas[key], "order") else None,
        )
    for msg, (keep_updated, order) in messages.items():
        msg_expr = cg.RawExpression(f"esphome::opentherm::MessageId::{msg}")
        if keep_updated:
            cg.add(hub.add_repeating_message(msg_expr))
        elif order is not None:
            cg.add(hub.add_initial_message(msg_expr, order))
        else:
            cg.add(hub.add_initial_message(msg_expr))


def add_property_set(var: cg.MockObj, config_key: str, config: dict[str, Any]) -> None:
    if config_key in config:
        cg.add(getattr(var, f"set_{config_key}")(config[config_key]))


Create = Callable[[dict[str, Any], str, cg.MockObj], Awaitable[cg.Pvariable]]


def create_only_conf(
    create: Callable[[dict[str, Any]], Awaitable[cg.Pvariable]],
) -> Create:
    return lambda conf, _key, _hub: create(conf)


async def component_to_code(
    component_type: str,
    schemas: dict[str, TSchema],
    type: cg.MockObjClass,
    create: Create,
    config: dict[str, Any],
) -> list[str]:
    """Generate the code for each configured component in the schema of a component type.

    Parameters:
    - component_type: The type of component, e.g. "sensor" or "binary_sensor"
    - schema_: The schema for that component type, a list of available components
    - type: The type of the component, e.g. sensor.Sensor or OpenthermOutput
    - create: A constructor function for the component, which receives the config,
      the key and the hub and should asynchronously return the new component
    - config: The configuration for this component type

    Returns: The list of keys for the created components
    """
    cg.add_define(f"OPENTHERM_USE_{component_type.upper()}")

    hub = await cg.get_variable(config[const.CONF_OPENTHERM_ID])

    keys: list[str] = []
    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf[CONF_ID]
        if id and id.type == type:
            entity = await create(conf, key, hub)
            if const.CONF_DATA_TYPE in conf:
                schemas[key].message_data = conf[const.CONF_DATA_TYPE]
            cg.add(getattr(hub, f"set_{key}_{component_type.lower()}")(entity))
            keys.append(key)

    define_has_component(component_type, keys)
    define_message_handler(component_type, keys, schemas)
    add_messages(hub, keys, schemas)

    return keys
