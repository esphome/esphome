from collections.abc import Callable
import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_ID,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_INTERNAL,
    CONF_NAME,
)
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj, add, get_variable
import esphome.final_validate as fv
from esphome.helpers import sanitize, snake_case
from esphome.types import ConfigType, EntityMetadata

_LOGGER = logging.getLogger(__name__)


def get_base_entity_object_id(
    name: str, friendly_name: str | None, device_name: str | None = None
) -> str:
    """Calculate the base object ID for an entity that will be set via set_object_id().

    This function calculates what object_id_c_str_ should be set to in C++.

    The C++ EntityBase::get_object_id() (entity_base.cpp lines 38-49) works as:
    - If !has_own_name && is_name_add_mac_suffix_enabled():
        return str_sanitize(str_snake_case(App.get_friendly_name()))  // Dynamic
    - Else:
        return object_id_c_str_ ?? ""  // What we set via set_object_id()

    Since we're calculating what to pass to set_object_id(), we always need to
    generate the object_id the same way, regardless of name_add_mac_suffix setting.

    Args:
        name: The entity name (empty string if no name)
        friendly_name: The friendly name from CORE.friendly_name
        device_name: The device name if entity is on a sub-device

    Returns:
        The base object ID to use for duplicate checking and to pass to set_object_id()
    """

    if name:
        # Entity has its own name (has_own_name will be true)
        base_str = name
    elif device_name:
        # Entity has empty name and is on a sub-device
        # C++ EntityBase::set_name() uses device->get_name() when device is set
        base_str = device_name
    elif friendly_name:
        # Entity has empty name (has_own_name will be false)
        # C++ uses App.get_friendly_name() which returns friendly_name or device name
        base_str = friendly_name
    else:
        # Fallback to device name
        base_str = CORE.name

    return sanitize(snake_case(base_str))


async def setup_entity(var: MockObj, config: ConfigType, platform: str) -> None:
    """Set up generic properties of an Entity.

    This function sets up the common entity properties like name, icon,
    entity category, etc.

    Args:
        var: The entity variable to set up
        config: Configuration dictionary containing entity settings
        platform: The platform name (e.g., "sensor", "binary_sensor")
    """
    # Get device info
    device_name: str | None = None
    device_id_obj: ID | None
    if device_id_obj := config.get(CONF_DEVICE_ID):
        device: MockObj = await get_variable(device_id_obj)
        add(var.set_device(device))
        # Get device name for object ID calculation
        device_name = device_id_obj.id

    # Calculate base object_id using the same logic as C++
    # This must match the C++ behavior in esphome/core/entity_base.cpp
    base_object_id = get_base_entity_object_id(
        config[CONF_NAME], CORE.friendly_name, device_name
    )

    if not config[CONF_NAME]:
        _LOGGER.debug(
            "Entity has empty name, using '%s' as object_id base", base_object_id
        )

    # Set both name and object_id in one call to reduce generated code size
    add(var.set_name_and_object_id(config[CONF_NAME], base_object_id))
    _LOGGER.debug(
        "Setting object_id '%s' for entity '%s' on platform '%s'",
        base_object_id,
        config[CONF_NAME],
        platform,
    )
    # Only set disabled_by_default if True (default is False)
    if config[CONF_DISABLED_BY_DEFAULT]:
        add(var.set_disabled_by_default(True))
    if CONF_INTERNAL in config:
        add(var.set_internal(config[CONF_INTERNAL]))
    if CONF_ICON in config:
        # Add USE_ENTITY_ICON define when icons are used
        cg.add_define("USE_ENTITY_ICON")
        add(var.set_icon(config[CONF_ICON]))
    if CONF_ENTITY_CATEGORY in config:
        add(var.set_entity_category(config[CONF_ENTITY_CATEGORY]))


def inherit_property_from(property_to_inherit, parent_id_property, transform=None):
    """Validator that inherits a configuration property from another entity, for use with FINAL_VALIDATE_SCHEMA.
    If a property is already set, it will not be inherited.
    Keyword arguments:
    property_to_inherit -- the name or path of the property to inherit, e.g. CONF_ICON or [CONF_SENSOR, 0, CONF_ICON]
                           (the parent must exist, otherwise nothing is done).
    parent_id_property -- the name or path of the property that holds the ID of the parent, e.g. CONF_POWER_ID or
                          [CONF_SENSOR, 1, CONF_POWER_ID].
    """

    def _walk_config(config, path):
        walk = [path] if not isinstance(path, list) else path
        for item_or_index in walk:
            config = config[item_or_index]
        return config

    def inherit_property(config):
        # Split the property into its path and name
        if not isinstance(property_to_inherit, list):
            property_path, property = [], property_to_inherit
        else:
            property_path, property = property_to_inherit[:-1], property_to_inherit[-1]

        # Check if the property to inherit is accessible
        try:
            config_part = _walk_config(config, property_path)
        except KeyError:
            return config

        # Only inherit the property if it does not exist yet
        if property not in config_part:
            fconf = fv.full_config.get()

            # Get config for the parent entity
            parent_id = _walk_config(config, parent_id_property)
            parent_path = fconf.get_path_for_id(parent_id)[:-1]
            parent_config = fconf.get_config_for_path(parent_path)

            # If parent sensor has the property set, inherit it
            if property in parent_config:
                path = fconf.get_path_for_id(config[CONF_ID])[:-1]
                this_config = _walk_config(
                    fconf.get_config_for_path(path), property_path
                )
                value = parent_config[property]
                if transform:
                    value = transform(value, config)
                this_config[property] = value

        return config

    return inherit_property


def entity_duplicate_validator(platform: str) -> Callable[[ConfigType], ConfigType]:
    """Create a validator function to check for duplicate entity names.

    This validator is meant to be used with schema.add_extra() for entity base schemas.

    Args:
        platform: The platform name (e.g., "sensor", "binary_sensor")

    Returns:
        A validator function that checks for duplicate names
    """

    def validator(config: ConfigType) -> ConfigType:
        if CONF_NAME not in config:
            # No name to validate
            return config

        # Skip validation for internal entities
        # Internal entities are not exposed to Home Assistant and don't use the hash-based
        # entity state tracking system, so name collisions don't matter for them
        if config.get(CONF_INTERNAL, False):
            return config

        # Get the entity name
        entity_name = config[CONF_NAME]

        # Get device name if entity is on a sub-device
        device_name = None
        device_id = ""  # Empty string for main device
        device_id_obj: ID | None
        if device_id_obj := config.get(CONF_DEVICE_ID):
            device_name = device_id_obj.id
            # Use the device ID string directly for uniqueness
            device_id = device_id_obj.id

        # Calculate what object_id will actually be used
        # This handles empty names correctly by using device/friendly names
        name_key = get_base_entity_object_id(
            entity_name, CORE.friendly_name, device_name
        )

        # Check for duplicates
        unique_key = (device_id, platform, name_key)
        if unique_key in CORE.unique_ids:
            # Get the existing entity metadata
            existing = CORE.unique_ids[unique_key]
            existing_name = existing.get("name", entity_name)
            existing_device = existing.get("device_id", "")
            existing_id = existing.get("entity_id", "unknown")

            # Build detailed error message
            device_prefix = f" on device '{device_id}'" if device_id else ""
            existing_device_prefix = (
                f" on device '{existing_device}'" if existing_device else ""
            )
            existing_component = existing.get("component", "unknown")

            # Provide more context about where the duplicate was found
            conflict_msg = (
                f"Conflicts with entity '{existing_name}'{existing_device_prefix}"
            )
            if existing_id != "unknown":
                conflict_msg += f" (id: {existing_id})"
            if existing_component != "unknown":
                conflict_msg += f" from component '{existing_component}'"

            # Show both original names and their ASCII-only versions if they differ
            sanitized_msg = ""
            if entity_name != existing_name:
                sanitized_msg = (
                    f"\n  Original names: '{entity_name}' and '{existing_name}'"
                    f"\n  Both convert to ASCII ID: '{name_key}'"
                    "\n  To fix: Add unique ASCII characters (e.g., '1', '2', or 'A', 'B')"
                    "\n          to distinguish them"
                )

            # Skip duplicate entity name validation when testing_mode is enabled
            # This flag is used for grouped component testing
            if not CORE.testing_mode:
                raise cv.Invalid(
                    f"Duplicate {platform} entity with name '{entity_name}' found{device_prefix}. "
                    f"{conflict_msg}. "
                    "Each entity on a device must have a unique name within its platform."
                    f"{sanitized_msg}"
                )

        # Store metadata about this entity
        entity_metadata: EntityMetadata = {
            "name": entity_name,
            "device_id": device_id,
            "platform": platform,
            "entity_id": str(config.get(CONF_ID, "unknown")),
            "component": CORE.current_component or "unknown",
        }

        # Add to tracking dict
        CORE.unique_ids[unique_key] = entity_metadata
        return config

    return validator
