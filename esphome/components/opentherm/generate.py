from collections.abc import Awaitable, Callable
from typing import Any

from esphome import core
import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.cpp_generator import StringLiteral

from . import const, schema
from .schema import EntitySchema, TSchema

opentherm_ns = cg.esphome_ns.namespace("opentherm")
message_data_ns = opentherm_ns.namespace("message_data")

MessageId = opentherm_ns.enum("MessageId")
MessageProcessor = opentherm_ns.class_("MessageProcessor")
OpenthermData = opentherm_ns.class_("OpenthermData")
OpenthermHub = opentherm_ns.class_("OpenthermHub", cg.Component)
OpenthermInputSensor = opentherm_ns.class_("OpenthermInputSensor", MessageProcessor)
OpenthermSetting = opentherm_ns.class_("OpenthermSetting", MessageProcessor)

TYPE_LIST = [
    "flag8_lb_0",
    "flag8_lb_1",
    "flag8_lb_2",
    "flag8_lb_3",
    "flag8_lb_4",
    "flag8_lb_5",
    "flag8_lb_6",
    "flag8_lb_7",
    "flag8_hb_0",
    "flag8_hb_1",
    "flag8_hb_2",
    "flag8_hb_3",
    "flag8_hb_4",
    "flag8_hb_5",
    "flag8_hb_6",
    "flag8_hb_7",
    "u8_lb",
    "u8_hb",
    "s8_lb",
    "s8_hb",
    "u8_lb_60",
    "u8_hb_60",
    "u16",
    "s16",
    "f88",
]
TYPE_MAP = {x: message_data_ns.class_(x) for x in TYPE_LIST}


def add_input_sensor(hub: cg.MockObj, key: str, input_sensor: cg.MockObj) -> None:
    input = schema.INPUTS[key]
    input_id = core.ID(
        f"{hub}_input_{key}",
        is_declaration=True,
        type=OpenthermInputSensor,
    )
    input_var = cg.new_Pvariable(input_id, accessor_template(input))
    cg.add(input_var.set_sensor(input_sensor))
    cg.add(input_var.set_id(StringLiteral(key)))
    cg.add(hub.register_message_processor(getattr(MessageId, input.message), input_var))


def add_setting(hub: cg.MockObj, key: str, value: Any) -> None:
    setting = schema.SETTINGS[key]
    setting_id = core.ID(
        f"{hub}_setting_{key}",
        is_declaration=True,
        type=OpenthermSetting,
    )
    setting_var = cg.new_Pvariable(setting_id, accessor_template(setting))
    cg.add(setting_var.set_value(value))
    cg.add(setting_var.set_id(StringLiteral(key)))
    cg.add(
        hub.register_message_processor(getattr(MessageId, setting.message), setting_var)
    )


def add_messages(hub: cg.MockObj, keys: list[str], schemas: dict[str, TSchema]):
    messages: dict[str, tuple[bool, int | None]] = {}
    for key in keys:
        messages[schemas[key].message] = (
            schemas[key].keep_updated,
            schemas[key].order if hasattr(schemas[key], "order") else None,
        )
    for msg, (keep_updated, order) in messages.items():
        msg_expr = getattr(MessageId, msg)
        if keep_updated:
            cg.add(hub.add_repeating_message(msg_expr))
        elif order is not None:
            cg.add(hub.add_initial_message(msg_expr, order))
        else:
            cg.add(hub.add_initial_message(msg_expr))


def add_property_set(var: cg.MockObj, config_key: str, config: dict[str, Any]) -> None:
    if config_key in config:
        cg.add(getattr(var, f"set_{config_key}")(config[config_key]))


def accessor_template(es: EntitySchema) -> cg.TemplateArguments:
    return cg.TemplateArguments(TYPE_MAP[es.message_data])


Create = Callable[[dict[str, Any], str, cg.MockObj], Awaitable[cg.MockObj]]


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
    hub = await cg.get_variable(config[const.CONF_OPENTHERM_ID])

    keys: list[str] = []
    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf[CONF_ID]
        if id and id.type == type:
            if const.CONF_DATA_TYPE in conf:
                schemas[key].message_data = conf[const.CONF_DATA_TYPE]
            entity = await create(conf, key, hub)
            cg.add(entity.set_id(StringLiteral(f"{key} ({id})")))
            cg.add(
                hub.register_message_processor(
                    getattr(MessageId, schemas[key].message), entity
                )
            )
            keys.append(key)

    add_messages(hub, keys, schemas)

    return keys
