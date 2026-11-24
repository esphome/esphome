"""Constant lists of the entities, devices and areas the Application holds.

Codegen records every object it adds to one of `App`'s lists, and a FINAL job emits each list as a
constant array of pointers (`esphome::app_lists::APP_<UPPER>_LIST`, declared in application.h), so the lists take no RAM.
Entities created at run time need slots reserved with `reserve_runtime_entities()`.
"""

from dataclasses import dataclass, field

from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.cpp_generator import (
    MockObj,
    RawStatement,
    add_define,
    add_global,
    static_pointer_address,
)

DOMAIN = "app_lists"

# Every entity platform with an App list, in entity_types.h order (a test keeps the two in sync).
ENTITY_PLATFORMS: tuple[str, ...] = (
    "binary_sensor",
    "cover",
    "fan",
    "light",
    "sensor",
    "switch",
    "button",
    "text_sensor",
    "climate",
    "number",
    "date",
    "time",
    "datetime",
    "text",
    "select",
    "lock",
    "valve",
    "media_player",
    "alarm_control_panel",
    "water_heater",
    "infrared",
    "radio_frequency",
    "event",
    "update",
)

# Platforms whose entity_types.h `upper` token and USE_* define carry a DATETIME_ prefix.
DATETIME_SUBTYPES = {"date", "time", "datetime"}


def _upper(platform: str) -> str:
    upper = platform.upper()
    return f"DATETIME_{upper}" if platform in DATETIME_SUBTYPES else upper


# Entity list name (the `singular` of entity_types.h) -> its `upper` token.
ENTITY_LISTS: dict[str, str] = {
    platform: _upper(platform) for platform in ENTITY_PLATFORMS
}


def use_define(platform: str) -> str:
    """The USE_* define that compiles in `platform`'s entity code."""
    return f"USE_{_upper(platform)}"


def _integration(platform: str) -> str:
    return "datetime" if platform in DATETIME_SUBTYPES else platform


@dataclass
class AppListsState:
    """Constant addresses of the objects added to each list, in emission order, keyed by `upper` token."""

    lists: dict[str, list[str]] = field(default_factory=dict)
    # Runtime slots reserved per platform
    reserved: dict[str, int] = field(default_factory=dict)
    emitted: bool = False


def _get_state() -> AppListsState:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = AppListsState()
    return CORE.data[DOMAIN]


def runtime_count_define(platform: str) -> str:
    return f"ESPHOME_RUNTIME_{ENTITY_LISTS[platform]}_COUNT"


def _check_not_emitted(what: str) -> AppListsState:
    if (state := _get_state()).emitted:
        raise EsphomeError(
            f"{what} after the lists were emitted; do it from to_code, not a FINAL job"
        )
    return state


def add_to_list(upper: str, var: MockObj) -> None:
    """Add `var` to the constant list `APP_<upper>_LIST`."""
    state = _check_not_emitted(f"{var} was added to the {upper} list")
    if (address := static_pointer_address(var)) is None:
        raise EsphomeError(
            f"{var} must be created with cg.new_Pvariable to be added to the {upper} list; "
            "an entity created at run time is registered with App.register_<platform>(obj, name) "
            "into slots reserved with cg.reserve_runtime_entities()"
        )
    state.lists.setdefault(upper, []).append(address)


def add_entity(platform: str, var: MockObj) -> None:
    """Add the entity `var` to the list of `platform` (for example `sensor`)."""
    add_to_list(ENTITY_LISTS[platform], var)


def entity_fields(
    *,
    device_class: str = "",
    unit_of_measurement: str = "",
    icon: str = "",
    entity_category: str = "",
    internal: bool = False,
    disabled_by_default: bool = False,
) -> int:
    """See esphome.core.entity_helpers.entity_fields; exported as cg.entity_fields."""
    # Imported here: entity_helpers imports codegen, which imports this module
    from esphome.core import entity_helpers

    return entity_helpers.entity_fields(
        device_class=device_class,
        unit_of_measurement=unit_of_measurement,
        icon=icon,
        entity_category=entity_category,
        internal=internal,
        disabled_by_default=disabled_by_default,
    )


def reserve_runtime_entities(platform: str, count: int) -> None:
    """Reserve `count` slots for `platform` entities created at run time.

    Each reservation adds one pointer of RAM. Without one, `App.register_<platform>()`
    does not exist, so a runtime registration fails to compile.
    """
    if platform not in ENTITY_LISTS:
        raise EsphomeError(f"Unknown entity platform '{platform}'")
    state = _check_not_emitted(f"Runtime {platform} slots were reserved")
    if (integration := _integration(platform)) not in CORE.loaded_integrations:
        raise EsphomeError(
            f"cg.reserve_runtime_entities('{platform}', ...) needs the {integration} "
            f'integration; add AUTO_LOAD = ["{integration}"] to the component'
        )
    state.reserved[platform] = state.reserved.get(platform, 0) + count


def _defined_values() -> dict[str, set[str]]:
    """Every define set through cg.add_define or a -D build flag, name -> values."""
    # Imported here: esphome.platformio pulls in config validation, which imports codegen
    from esphome.platformio.library import lex_build_flags

    values: dict[str, set[str]] = {}
    for define in CORE.defines:
        values.setdefault(define.name, set()).add(str(define.value))
    for flag in lex_build_flags(sorted(CORE.build_flags), "build_flags"):
        if flag.startswith("-D"):
            name, _, value = flag[2:].partition("=")
            values.setdefault(name, set()).add(value)
    return values


def _add_count_define(
    defined: dict[str, set[str]],
    name: str,
    value: int,
    reason: str = "entities cannot be added at run time",
) -> None:
    """Emit a list size, refusing a different value defined elsewhere: it would size the walk wrong."""
    if others := sorted(defined.get(name, set()) - {str(value)}):
        raise EsphomeError(
            f"{name} is defined as {', '.join(others)} elsewhere, but codegen sizes it as "
            f"{value}. Remove that define; {reason}."
        )
    add_define(name, value)


def _emit_list(upper: str, objs: list[str]) -> None:
    name = f"esphome::app_lists::APP_{upper}_LIST"
    # A list without codegen objects still needs a definition; its count is 0, so it is never read
    items = ", ".join(objs) or "nullptr"
    add_global(RawStatement(f"decltype({name}) {name} PROGMEM = {{{items}}};"))


@coroutine_with_priority(CoroPriority.FINAL)
async def _emit_entity_lists() -> None:
    state = _get_state()
    state.emitted = True
    defined = _defined_values()
    for platform, upper in ENTITY_LISTS.items():
        objs = state.lists.get(upper, [])
        reserved = state.reserved.get(platform, 0)
        # The one place that decides whether the type is compiled in, so USE_* and the list agree;
        # a USE_* defined directly (tests, benchmarks) counts as enabled
        if (
            not reserved
            and not CORE.platform_counts.get(platform)
            and use_define(platform) not in defined
        ):
            continue
        add_define(use_define(platform))
        if platform in ("infrared", "radio_frequency"):
            add_define("USE_IR_RF")
        if reserved:
            add_define("USE_RUNTIME_ENTITIES")
        hint = (
            f"to register {platform} entities at run time, call "
            f'cg.reserve_runtime_entities("{platform}", n) in to_code'
        )
        _add_count_define(
            defined, f"ESPHOME_ENTITY_{platform.upper()}_COUNT", len(objs), hint
        )
        _add_count_define(defined, runtime_count_define(platform), reserved, hint)
        _emit_list(upper, objs)
    for upper in ("DEVICE", "AREA"):
        if (objs := state.lists.get(upper)) is not None:
            _add_count_define(
                defined,
                f"ESPHOME_{upper}_COUNT",
                len(objs),
                "devices and areas come from the YAML devices: and areas: lists",
            )
            _emit_list(upper, objs)


def schedule_emit() -> None:
    """Schedule the FINAL job that emits the lists; core's to_code calls it once per run."""
    CORE.add_job(_emit_entity_lists)
