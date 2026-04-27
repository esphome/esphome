from collections.abc import Callable
from dataclasses import dataclass, field
import functools
import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_DEVICE_ID,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_INTERNAL,
    CONF_NAME,
    CONF_UNIT_OF_MEASUREMENT,
)
from esphome.core import CORE, ID, CoroPriority, coroutine_with_priority
from esphome.core.config import (
    DEVICE_CLASS_MAX_LENGTH,
    ICON_MAX_LENGTH,
    UNIT_OF_MEASUREMENT_MAX_LENGTH,
)
from esphome.cpp_generator import MockObj, RawStatement, add, get_variable
from esphome.cpp_types import App
import esphome.final_validate as fv
from esphome.helpers import cpp_string_escape, fnv1_hash_object_id, sanitize, snake_case
from esphome.types import ConfigType, EntityMetadata

_LOGGER = logging.getLogger(__name__)

DOMAIN = "entity_string_pool"

# Private config keys for storing registered string indices
_KEY_DC_IDX = "_entity_dc_idx"
_KEY_UOM_IDX = "_entity_uom_idx"
_KEY_ICON_IDX = "_entity_icon_idx"
_KEY_ENTITY_NAME = "_entity_name"
_KEY_OBJECT_ID_HASH = "_entity_object_id_hash"

# Bit layout for entity_fields in configure_entity_().
# Keep in sync with ENTITY_FIELD_*_SHIFT constants in esphome/core/entity_base.h
_DC_SHIFT = 0
_UOM_SHIFT = 8
_ICON_SHIFT = 16
_INTERNAL_SHIFT = 24
_DISABLED_BY_DEFAULT_SHIFT = 25
_ENTITY_CATEGORY_SHIFT = 26

# Private config keys for storing flags
_KEY_INTERNAL = "_entity_internal"
_KEY_DISABLED_BY_DEFAULT = "_entity_disabled_by_default"
_KEY_ENTITY_CATEGORY = "_entity_category"

# Private config key for the App.register_<method> entry point.
# When set, finalize_entity_strings() emits a single combined call
# `App.register_<method>(var, name, hash, packed)` instead of separate
# `App.register_<method>(var)` and `var->configure_entity_(...)` calls.
_KEY_REGISTER_METHOD = "_entity_register_method"

# Maximum unique strings per category (8-bit index, 0 = not set)
_MAX_DEVICE_CLASSES = 0xFF  # 255
_MAX_UNITS = 0xFF  # 255
_MAX_ICONS = 0xFF  # 255


@dataclass
class EntityStringPool:
    """Pool of entity string properties for PROGMEM pointer tables.

    Strings are registered during to_code() and assigned 1-based indices.
    Index 0 means "not set" (empty string). At render time, the pool
    generates C++ PROGMEM pointer table + lookup function per category.
    """

    device_classes: dict[str, int] = field(default_factory=dict)
    units: dict[str, int] = field(default_factory=dict)
    icons: dict[str, int] = field(default_factory=dict)
    tables_registered: bool = False


def _get_pool() -> EntityStringPool:
    """Get or create the entity string pool from CORE.data."""
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = EntityStringPool()
    return CORE.data[DOMAIN]


def _ensure_tables_registered() -> None:
    """Schedule the table generation job (once)."""
    pool = _get_pool()
    if pool.tables_registered:
        return
    pool.tables_registered = True
    CORE.add_job(_generate_tables_job)


def _generate_category_code(
    table_var: str,
    lookup_fn: str,
    strings: dict[str, int],
    *,
    progmem_strings: bool = False,
) -> str:
    """Generate C++ code for one string category (PROGMEM pointer table + lookup).

    Uses a PROGMEM array of string pointers. On ESP8266, pointers are stored
    in flash (via PROGMEM) and read with progmem_read_ptr(). String literals
    themselves remain in RAM but benefit from linker string deduplication.
    Index 0 means "not set" and returns empty string.

    When progmem_strings=True, each string is declared as a separate PROGMEM
    char array. This ensures the string data itself is in flash on ESP8266
    (where .rodata is RAM). On other platforms PROGMEM is a no-op.
    """
    if not strings:
        return ""

    sorted_strings = sorted(strings.items(), key=lambda x: x[1])
    count = len(sorted_strings)

    if progmem_strings:
        # Emit individual PROGMEM char arrays so string data lives in flash
        lines: list[str] = []
        var_names: list[str] = []
        for i, (s, _) in enumerate(sorted_strings):
            var_name = f"{table_var}_STR_{i}"
            var_names.append(var_name)
            lines.append(
                f"static const char {var_name}[] PROGMEM = {cpp_string_escape(s)};"
            )
        entries = ", ".join(var_names)
        # Empty string must also be PROGMEM — on ESP8266, callers use strncpy_P
        empty_var = f"{table_var}_EMPTY"
        lines.append(f'static const char {empty_var}[] PROGMEM = "";')
        lines.append(f"static const char *const {table_var}[] PROGMEM = {{{entries}}};")
        lines.append(f"const char *{lookup_fn}(uint8_t index) {{")
        lines.append(f"  if (index == 0 || index > {count}) return {empty_var};")
        lines.append(f"  return progmem_read_ptr(&{table_var}[index - 1]);")
        lines.append("}")
        return "\n".join(lines) + "\n"

    entries = ", ".join(cpp_string_escape(s) for s, _ in sorted_strings)

    return (
        f"static const char *const {table_var}[] PROGMEM = {{{entries}}};\n"
        f"const char *{lookup_fn}(uint8_t index) {{\n"
        f'  if (index == 0 || index > {count}) return "";\n'
        f"  return progmem_read_ptr(&{table_var}[index - 1]);\n"
        f"}}\n"
    )


_CATEGORY_CONFIGS = (
    ("ENTITY_DC_TABLE", "entity_device_class_lookup", "device_classes", True),
    ("ENTITY_UOM_TABLE", "entity_uom_lookup", "units", False),
    ("ENTITY_ICON_TABLE", "entity_icon_lookup", "icons", True),
)


@coroutine_with_priority(CoroPriority.FINAL)
async def _generate_tables_job() -> None:
    """Generate all entity string table C++ code as a FINAL-priority job.

    Runs after all component to_code() calls have registered their strings.
    """
    pool = _get_pool()
    parts = ["namespace esphome {"]
    for table_var, lookup_fn, attr, progmem_strs in _CATEGORY_CONFIGS:
        code = _generate_category_code(
            table_var, lookup_fn, getattr(pool, attr), progmem_strings=progmem_strs
        )
        if code:
            parts.append(code)
    parts.append("}  // namespace esphome")
    cg.add_global(RawStatement("\n".join(parts)))


def _register_string(
    value: str, category: dict[str, int], max_count: int, category_name: str
) -> int:
    """Register a string in a category dict and return its 1-based index.

    Returns 0 if value is empty/None (meaning "not set").
    """
    if not value:
        return 0
    if value in category:
        return category[value]
    idx = len(category) + 1
    if idx > max_count:
        raise ValueError(
            f"Too many unique {category_name} values (max {max_count}), got {idx}: '{value}'"
        )
    category[value] = idx
    _ensure_tables_registered()
    return idx


def register_device_class(value: str) -> int:
    """Register a device_class string and return its 1-based index."""
    byte_len = len(value.encode("utf-8")) if value else 0
    if byte_len > DEVICE_CLASS_MAX_LENGTH:
        raise ValueError(
            f"Device class string too long ({byte_len} bytes, max {DEVICE_CLASS_MAX_LENGTH}): '{value}'"
        )
    return _register_string(
        value, _get_pool().device_classes, _MAX_DEVICE_CLASSES, "device_class"
    )


def register_unit_of_measurement(value: str) -> int:
    """Register a unit_of_measurement string and return its 1-based index."""
    byte_len = len(value.encode("utf-8")) if value else 0
    if byte_len > UNIT_OF_MEASUREMENT_MAX_LENGTH:
        raise ValueError(
            f"Unit of measurement string too long ({byte_len} bytes, "
            f"max {UNIT_OF_MEASUREMENT_MAX_LENGTH}): '{value}'"
        )
    return _register_string(value, _get_pool().units, _MAX_UNITS, "unit_of_measurement")


def register_icon(value: str) -> int:
    """Register an icon string and return its 1-based index."""
    byte_len = len(value.encode("utf-8")) if value else 0
    if byte_len > ICON_MAX_LENGTH:
        raise ValueError(
            f"Icon string too long ({byte_len} bytes, max {ICON_MAX_LENGTH}): '{value}'"
        )
    return _register_string(value, _get_pool().icons, _MAX_ICONS, "icon")


def setup_device_class(config: ConfigType) -> None:
    """Register config's device_class and store its index for finalize_entity_strings."""
    idx = register_device_class(config.get(CONF_DEVICE_CLASS, ""))
    if idx:
        cg.add_define("USE_ENTITY_DEVICE_CLASS")
    config[_KEY_DC_IDX] = idx


def setup_unit_of_measurement(config: ConfigType) -> None:
    """Register config's unit_of_measurement and store its index for finalize_entity_strings."""
    idx = register_unit_of_measurement(config.get(CONF_UNIT_OF_MEASUREMENT, ""))
    if idx:
        cg.add_define("USE_ENTITY_UNIT_OF_MEASUREMENT")
    config[_KEY_UOM_IDX] = idx


def _sanitize_comment(text: str) -> str:
    r"""Sanitize a string for safe inclusion in a C++ // line comment.

    Dangerous characters:
    - \n, \r: break out of line comment, next line becomes code
    - \: at end of line, splices next line into comment (eats real code)
    """
    return text.replace("\\", "/").replace("\n", " ").replace("\r", "")


def _describe_packed_flags(config: ConfigType, entity_category: int) -> str:
    """Build a human-readable description of packed entity flags for C++ comments."""
    parts: list[str] = []
    if config.get(_KEY_INTERNAL):
        parts.append("internal")
    if config.get(_KEY_DISABLED_BY_DEFAULT):
        parts.append("disabled_by_default")
    entity_cat_keys = list(cv.ENTITY_CATEGORIES)
    if entity_category < len(entity_cat_keys) and (
        cat_name := entity_cat_keys[entity_category]
    ):
        parts.append(f"category:{cat_name}")
    if config.get(_KEY_DC_IDX) and (dc := config.get(CONF_DEVICE_CLASS)):
        parts.append(f"dc:{_sanitize_comment(dc)}")
    if config.get(_KEY_UOM_IDX) and (uom := config.get(CONF_UNIT_OF_MEASUREMENT)):
        parts.append(f"uom:{_sanitize_comment(uom)}")
    if config.get(_KEY_ICON_IDX) and (icon := config.get(CONF_ICON)):
        parts.append(f"icon:{_sanitize_comment(icon)}")
    return ", ".join(parts)


def queue_entity_register(method_name: str, config: ConfigType) -> None:
    """Defer ``App.register_<method_name>(var)`` emission to ``finalize_entity_strings``.

    When the deferred call is emitted, it is folded with ``configure_entity_`` into
    a single ``App.register_<method_name>(var, name, hash, packed)`` call site,
    which removes one statement and one method dispatch per entity from the
    generated ``main.cpp``.
    """
    config[_KEY_REGISTER_METHOD] = method_name


def finalize_entity_strings(var: MockObj, config: ConfigType) -> None:
    """Emit the entity-registration / configure_entity_ tail.

    Call this at the end of each component's setup function, after
    setup_entity() and any register_device_class/register_unit_of_measurement calls.

    If queue_entity_register() was called for this entity, emits one combined call
    ``App.register_<method>(var, name, hash, packed)``. Otherwise falls back to a
    standalone ``var->configure_entity_(name, hash, packed)``.
    """
    entity_name = config[_KEY_ENTITY_NAME]
    object_id_hash = config[_KEY_OBJECT_ID_HASH]
    dc_idx = config.get(_KEY_DC_IDX, 0)
    uom_idx = config.get(_KEY_UOM_IDX, 0)
    icon_idx = config.get(_KEY_ICON_IDX, 0)
    internal = config.get(_KEY_INTERNAL, 0)
    disabled_by_default = config.get(_KEY_DISABLED_BY_DEFAULT, 0)
    entity_category = config.get(_KEY_ENTITY_CATEGORY, 0)
    packed = (
        (dc_idx << _DC_SHIFT)
        | (uom_idx << _UOM_SHIFT)
        | (icon_idx << _ICON_SHIFT)
        | (internal << _INTERNAL_SHIFT)
        | (disabled_by_default << _DISABLED_BY_DEFAULT_SHIFT)
        | (entity_category << _ENTITY_CATEGORY_SHIFT)
    )
    # Build inline comment describing the packed flags for readability
    comment = _describe_packed_flags(config, entity_category)
    register_method = config.get(_KEY_REGISTER_METHOD)
    if register_method is not None:
        expr = getattr(App, f"register_{register_method}")(
            var, entity_name, object_id_hash, packed
        )
    else:
        expr = var.configure_entity_(entity_name, object_id_hash, packed)
    if comment:
        add(RawStatement(f"{expr};  // {comment}"))
    else:
        add(expr)


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


def setup_entity(var_or_platform, config=None, platform=None):
    """Set up entity properties — works as both decorator and direct call.

    Decorator mode::

        @setup_entity("sensor")
        async def setup_sensor_core_(var, config):
            setup_device_class(config)
            setup_unit_of_measurement(config)
            ...

    Direct call mode (for entities with no extra string properties)::

        await setup_entity(var, config, "camera")
    """
    if isinstance(var_or_platform, str) and config is None:
        # Decorator mode: @setup_entity("sensor")
        platform = var_or_platform

        def decorator(func: Callable) -> Callable:
            @functools.wraps(func)
            async def wrapper(
                var: MockObj, config: ConfigType, *args, **kwargs
            ) -> None:
                await _setup_entity_impl(var, config, platform)
                await func(var, config, *args, **kwargs)
                finalize_entity_strings(var, config)

            return wrapper

        return decorator

    # Direct call mode: await setup_entity(var, config, "camera")
    async def _do() -> None:
        await _setup_entity_impl(var_or_platform, config, platform)
        finalize_entity_strings(var_or_platform, config)

    return _do()


async def _setup_entity_impl(var: MockObj, config: ConfigType, platform: str) -> None:
    """Set up generic properties of an Entity (internal implementation).

    This function sets up the common entity properties like name, icon,
    entity category, etc.

    Args:
        var: The entity variable to set up
        config: Configuration dictionary containing entity settings
        platform: The platform name (e.g., "sensor", "binary_sensor")
    """
    # Get device info if configured
    if device_id_obj := config.get(CONF_DEVICE_ID):
        device: MockObj = await get_variable(device_id_obj)
        add(var.set_device_(device))

    # Pre-compute entity name and object_id hash for configure_entity_()
    # which is emitted later by finalize_entity_strings().
    # For named entities: pre-compute hash from entity name
    # For empty-name entities: pass 0, C++ calculates hash at runtime from
    # device name, friendly_name, or app name (bug-for-bug compatibility)
    entity_name = config[CONF_NAME]
    object_id_hash = fnv1_hash_object_id(entity_name) if entity_name else 0
    config[_KEY_ENTITY_NAME] = entity_name
    config[_KEY_OBJECT_ID_HASH] = object_id_hash
    # Store flags for packing into configure_entity_()
    config[_KEY_DISABLED_BY_DEFAULT] = int(config[CONF_DISABLED_BY_DEFAULT])
    if CONF_INTERNAL in config:
        config[_KEY_INTERNAL] = int(config[CONF_INTERNAL])
    icon_idx = 0
    if CONF_ICON in config:
        # Add USE_ENTITY_ICON define when icons are used
        cg.add_define("USE_ENTITY_ICON")
        icon_idx = register_icon(config[CONF_ICON])
    if CONF_ENTITY_CATEGORY in config:
        # Derive integer value from key position in cv.ENTITY_CATEGORIES
        # (must match C++ EntityCategory enum in entity_base.h)
        entity_cat_str = str(config[CONF_ENTITY_CATEGORY])
        entity_cat_keys = list(cv.ENTITY_CATEGORIES)
        config[_KEY_ENTITY_CATEGORY] = (
            entity_cat_keys.index(entity_cat_str)
            if entity_cat_str in entity_cat_keys
            else 0
        )
    # Store icon index for finalize_entity_strings
    config[_KEY_ICON_IDX] = icon_idx


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
