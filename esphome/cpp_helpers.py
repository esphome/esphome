import logging

from esphome.const import (
    CONF_CONFIGURATION_URL,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_IDENTIFIERS,
    CONF_INTERNAL,
    CONF_MANUFACTURER,
    CONF_MODEL,
    CONF_NAME,
    CONF_SETUP_PRIORITY,
    CONF_SOFTWARE_VERSION,
    CONF_SUGGESTED_AREA,
    CONF_UPDATE_INTERVAL,
    CONF_TYPE_ID,
)

# pylint: disable=unused-import
from esphome.core import coroutine, ID, CORE
from esphome.types import ConfigType
from esphome.cpp_generator import RawExpression, add, get_variable
from esphome.cpp_types import App
from esphome.util import Registry, RegistryEntry


_LOGGER = logging.getLogger(__name__)


async def gpio_pin_expression(conf):
    """Generate an expression for the given pin option.

    This is a coroutine, you must await it with a 'await' expression!
    """
    if conf is None:
        return None
    from esphome import pins

    for key, (func, _) in pins.PIN_SCHEMA_REGISTRY.items():
        if key in conf:
            return await coroutine(func)(conf)
    return await coroutine(pins.PIN_SCHEMA_REGISTRY[CORE.target_platform][0])(conf)


async def register_component(var, config):
    """Register the given obj as a component.

    This is a coroutine, you must await it with a 'await' expression!

    :param var: The variable representing the component.
    :param config: The configuration for the component.
    """
    import inspect

    id_ = str(var.base)
    if id_ not in CORE.component_ids:
        raise ValueError(
            f"Component ID {id_} was not declared to inherit from Component, or was registered twice. Please create a bug report with your configuration."
        )
    CORE.component_ids.remove(id_)
    if CONF_SETUP_PRIORITY in config:
        add(var.set_setup_priority(config[CONF_SETUP_PRIORITY]))
    if CONF_UPDATE_INTERVAL in config:
        add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))

    # Set component source by inspecting the stack and getting the callee module
    # https://stackoverflow.com/a/1095621
    name = None
    try:
        for frm in inspect.stack()[1:]:
            mod = inspect.getmodule(frm[0])
            if mod is None:
                continue
            name = mod.__name__
            if name.startswith("esphome.components."):
                name = name[len("esphome.components.") :]
                break
            if name == "esphome.automation":
                name = "automation"
                # continue looking further up in stack in case we find a better one
            if name == "esphome.coroutine":
                # Only works for async-await coroutine syntax
                break
    except (KeyError, AttributeError, IndexError) as e:
        _LOGGER.warning(
            "Error while finding name of component, please report this", exc_info=e
        )
    if name is not None:
        add(var.set_component_source(name))

    add(App.register_component(var))
    return var


async def register_parented(var, value):
    if isinstance(value, ID):
        paren = await get_variable(value)
    else:
        paren = value
    add(var.set_parent(paren))


async def setup_entity(var, config):
    """Set up generic properties of an Entity"""
    add(var.set_name(config[CONF_NAME]))
    add(var.set_disabled_by_default(config[CONF_DISABLED_BY_DEFAULT]))
    if CONF_INTERNAL in config:
        add(var.set_internal(config[CONF_INTERNAL]))
    if CONF_ICON in config:
        add(var.set_icon(config[CONF_ICON]))
    if CONF_ENTITY_CATEGORY in config:
        add(var.set_entity_category(config[CONF_ENTITY_CATEGORY]))


async def setup_device_registry_entry(var, config):
    """Sets up a device registry entry"""

    # Defaults
    if CONF_NAME not in config:
        config[CONF_NAME] = RawExpression("App.get_name()")

    if CONF_MODEL not in config:
        config[CONF_MODEL] = RawExpression("ESPHOME_BOARD")

    if CONF_SOFTWARE_VERSION not in config:
        config[CONF_SOFTWARE_VERSION] = RawExpression(
            '"esphome v" ESPHOME_VERSION " " + App.get_compilation_time()'
        )

    if CONF_IDENTIFIERS not in config:
        config[CONF_IDENTIFIERS] = RawExpression("{get_mac_address()}")

    property_map = {
        CONF_NAME: var.set_name,
        CONF_IDENTIFIERS: var.add_identifier,
        CONF_MANUFACTURER: var.set_manufacturer,
        CONF_MODEL: var.set_model,
        CONF_SOFTWARE_VERSION: var.set_software_version,
        CONF_SUGGESTED_AREA: var.set_suggested_area,
        CONF_CONFIGURATION_URL: var.set_configuration_url,
    }

    for property, func in property_map.items():
        if property in config:
            if isinstance(config[property], list):
                for list_item in config[property]:
                    add(func(list_item))
            else:
                add(func(config[property]))


def extract_registry_entry_config(registry, full_config):
    # type: (Registry, ConfigType) -> RegistryEntry
    key, config = next((k, v) for k, v in full_config.items() if k in registry)
    return registry[key], config


async def build_registry_entry(registry, full_config):
    registry_entry, config = extract_registry_entry_config(registry, full_config)
    type_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    return await builder(config, type_id)


async def build_registry_list(registry, config):
    actions = []
    for conf in config:
        action = await build_registry_entry(registry, conf)
        actions.append(action)
    return actions
