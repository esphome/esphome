from dataclasses import dataclass, field
import logging

from esphome.const import (
    CONF_SAFE_MODE,
    CONF_SETUP_PRIORITY,
    CONF_TYPE_ID,
    CONF_UPDATE_INTERVAL,
    KEY_PAST_SAFE_MODE,
)
from esphome.core import CORE, ID, CoroPriority, coroutine, coroutine_with_priority
from esphome.coroutine import FakeAwaitable
from esphome.cpp_generator import (
    RawStatement,
    add,
    add_define,
    add_global,
    get_variable,
)
from esphome.cpp_types import App
from esphome.helpers import cpp_string_escape
from esphome.types import ConfigFragmentType, ConfigType
from esphome.util import Registry, RegistryEntry

_LOGGER = logging.getLogger(__name__)

_COMPONENT_SOURCE_DOMAIN = "component_source_pool"

# Maximum unique component source names (8-bit index, 0 = not set)
_MAX_COMPONENT_SOURCES = 0xFF  # 255


@dataclass
class ComponentSourcePool:
    """Pool of component source names for PROGMEM lookup table.

    Source names are registered during to_code() and assigned 1-based indices.
    Index 0 means "not set" (returns LOG_STR("<unknown>")). At render time,
    the pool generates a C++ PROGMEM table + lookup function.
    """

    sources: dict[str, int] = field(default_factory=dict)
    table_registered: bool = False


def _get_source_pool() -> ComponentSourcePool:
    """Get or create the component source pool from CORE.data."""
    if _COMPONENT_SOURCE_DOMAIN not in CORE.data:
        CORE.data[_COMPONENT_SOURCE_DOMAIN] = ComponentSourcePool()
    return CORE.data[_COMPONENT_SOURCE_DOMAIN]


def _ensure_source_table_registered() -> None:
    """Schedule the table generation job (once)."""
    pool = _get_source_pool()
    if pool.table_registered:
        return
    pool.table_registered = True
    CORE.add_job(_generate_component_source_table)


def register_component_source(name: str) -> int:
    """Register a component source name and return its 1-based index.

    Deduplicates: multiple components from the same source share one index.
    """
    if not name:
        return 0
    pool = _get_source_pool()
    if name in pool.sources:
        return pool.sources[name]
    idx = len(pool.sources) + 1
    if idx > _MAX_COMPONENT_SOURCES:
        if not CORE.testing_mode:
            _LOGGER.warning(
                "Too many unique component source names (max %d), "
                "'%s' will show as '<unknown>'",
                _MAX_COMPONENT_SOURCES,
                name,
            )
        return 0
    pool.sources[name] = idx
    _ensure_source_table_registered()
    return idx


def _generate_source_table_code(
    table_var: str,
    lookup_fn: str,
    strings: dict[str, int],
) -> str:
    """Generate C++ PROGMEM table + LogString* lookup for component sources.

    Same pattern as entity_helpers._generate_category_code but returns
    const LogString* instead of const char* (needed for LOG_STR_ARG).
    """
    if not strings:
        return ""

    sorted_strings = sorted(strings.items(), key=lambda x: x[1])
    count = len(sorted_strings)

    # Emit individual PROGMEM char arrays so string data lives in flash on ESP8266
    lines: list[str] = []
    var_names: list[str] = []
    for i, (s, _) in enumerate(sorted_strings):
        var_name = f"{table_var}_STR_{i}"
        var_names.append(var_name)
        lines.append(
            f"static const char {var_name}[] PROGMEM = {cpp_string_escape(s)};"
        )

    entries = ", ".join(var_names)
    lines.append(f"static const char *const {table_var}[] PROGMEM = {{{entries}}};")
    lines.append(f"const LogString *{lookup_fn}(uint8_t index) {{")
    cond = "index == 0" if count >= 255 else f"index == 0 || index > {count}"
    lines.append(f'  if ({cond}) return LOG_STR("<unknown>");')
    lines.append("  return reinterpret_cast<const LogString *>(")
    lines.append(f"    progmem_read_ptr(&{table_var}[index - 1]));")
    lines.append("}")
    return "\n".join(lines) + "\n"


@coroutine_with_priority(CoroPriority.FINAL)
async def _generate_component_source_table() -> None:
    """Generate the component source lookup table as a FINAL-priority job.

    Runs after all component to_code() calls have registered their sources.
    """
    pool = _get_source_pool()
    if code := _generate_source_table_code(
        "COMP_SRC_TABLE", "component_source_lookup", pool.sources
    ):
        add_global(
            RawStatement(f"namespace esphome {{\n{code}}}  // namespace esphome")
        )


async def gpio_pin_expression(conf):
    """Generate an expression for the given pin option.

    This is a coroutine, you must await it with a 'await' expression!
    """
    if conf is None:
        return None
    from esphome import pins

    for key, (func, _, _) in pins.PIN_SCHEMA_REGISTRY.items():
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
        add_define("USE_SETUP_PRIORITY_OVERRIDE")
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
        idx = register_component_source(name)
        add(App.register_component_(var, idx))
    else:
        add(App.register_component_(var))

    # Collect C++ type for compile-time looping component count
    comp_entries = CORE.data.setdefault("looping_component_entries", [])
    comp_entries.append(str(var.base.type))

    return var


async def register_parented(var, value):
    if isinstance(value, ID):
        paren = await get_variable(value)
    else:
        paren = value
    add(var.set_parent(paren))


def extract_registry_entry_config(
    registry: Registry,
    full_config: ConfigType,
) -> tuple[RegistryEntry, ConfigFragmentType]:
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


async def past_safe_mode():
    if CONF_SAFE_MODE not in CORE.config:
        return None

    def _safe_mode_generator():
        while True:
            if CORE.data.get(CONF_SAFE_MODE, {}).get(KEY_PAST_SAFE_MODE, False):
                return
            yield

    return await FakeAwaitable(_safe_mode_generator())
