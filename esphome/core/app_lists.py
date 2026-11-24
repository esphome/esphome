"""Constant lists of the entities, devices and areas the Application holds.

Codegen records every object it adds to one of `App`'s lists, and a FINAL job emits each list as a
constant array of pointers (`APP_<UPPER>_LIST`, declared in application.h), so the lists take no RAM.
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
from esphome.cpp_helpers import get_slot_count, slot_counter

DOMAIN = "app_lists"

# Platforms whose entity_types.h `upper` token and USE_* define carry a DATETIME_ prefix.
DATETIME_SUBTYPES = {"date", "time", "datetime"}


def _upper(platform: str) -> str:
    upper = platform.upper()
    return f"DATETIME_{upper}" if platform in DATETIME_SUBTYPES else upper


# Entity list name (the `singular` of entity_types.h) -> its `upper` token.
ENTITY_LISTS: dict[str, str] = {
    platform: _upper(platform)
    for platform in (
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
}


def use_define(platform: str) -> str:
    """The USE_* define that compiles in `platform`'s entity code."""
    return f"USE_{_upper(platform)}"


# Defines an entity type needs besides its own USE_*, emitted with its list.
_SHARED_DEFINES: dict[str, str] = {
    "infrared": "USE_IR_RF",
    "radio_frequency": "USE_IR_RF",
}


def _integration(platform: str) -> str:
    return "datetime" if platform in DATETIME_SUBTYPES else platform


@dataclass
class AppListsState:
    """Constant addresses of the objects added to each list, in emission order, keyed by `upper` token."""

    lists: dict[str, list[str]] = field(default_factory=dict)
    emitted: bool = False


def _get_state() -> AppListsState:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = AppListsState()
    return CORE.data[DOMAIN]


def runtime_count_define(platform: str) -> str:
    return f"ESPHOME_RUNTIME_{ENTITY_LISTS[platform]}_COUNT"


def add_to_list(upper: str, var: MockObj) -> None:
    """Add `var` to the constant list `APP_<upper>_LIST`."""
    state = _get_state()
    if state.emitted:
        raise EsphomeError(
            f"{var} was added to the {upper} list after the lists were emitted"
        )
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


def reserve_runtime_entities(platform: str, count: int) -> None:
    """Reserve `count` slots for `platform` entities created at run time.

    Each reservation adds one pointer of RAM. Without one, `App.register_<platform>()`
    does not exist, so a runtime registration fails to compile.
    """
    if platform not in ENTITY_LISTS:
        raise EsphomeError(f"Unknown entity platform '{platform}'")
    if _get_state().emitted:
        raise EsphomeError(
            f"Runtime {platform} slots were reserved after the lists were emitted; "
            "call cg.reserve_runtime_entities() from to_code"
        )
    if (integration := _integration(platform)) not in CORE.loaded_integrations:
        raise EsphomeError(
            f"cg.reserve_runtime_entities('{platform}', ...) needs the {integration} "
            f'integration; add AUTO_LOAD = ["{integration}"] to the component'
        )
    request_slot = slot_counter(runtime_count_define(platform))
    for _ in range(count):
        request_slot()


def _defined_values(name: str) -> set[str]:
    """Values `name` is already defined with, through cg.add_define or a -D build flag."""
    values = {str(define.value) for define in CORE.defines if define.name == name}
    values.update(
        flag.partition("=")[2]
        for flag in CORE.build_flags
        if flag == f"-D{name}" or flag.startswith(f"-D{name}=")
    )
    return values


def _check_count_define(name: str, value: int, platform: str) -> None:
    """Refuse a list size defined elsewhere with a different value: it would size the walk wrong."""
    if others := sorted(_defined_values(name) - {str(value)}):
        raise EsphomeError(
            f"{name} is defined as {', '.join(others)} elsewhere, but codegen sizes it as "
            f"{value}. Remove that define; to register {platform} entities at run time, call "
            f'cg.reserve_runtime_entities("{platform}", n) in to_code'
        )


def _add_count_define(name: str, value: int, platform: str) -> None:
    _check_count_define(name, value, platform)
    add_define(name, value)


def _emit_list(upper: str, objs: list[str]) -> None:
    name = f"esphome::APP_{upper}_LIST"
    # A list without codegen objects still needs a definition; its count is 0, so it is never read
    items = ", ".join(objs) or "nullptr"
    add_global(RawStatement(f"decltype({name}) {name} PROGMEM = {{{items}}};"))


@coroutine_with_priority(CoroPriority.FINAL)
async def _emit_entity_lists() -> None:
    state = _get_state()
    state.emitted = True
    for platform, upper in ENTITY_LISTS.items():
        objs = state.lists.get(upper, [])
        if reserved := get_slot_count(runtime_count_define(platform)):
            add_define("USE_RUNTIME_ENTITIES")
        # The one place that decides whether the type is compiled in, so USE_* and the list agree;
        # a USE_* defined directly (tests, benchmarks) counts as enabled
        if (
            not reserved
            and not CORE.platform_counts.get(platform)
            and not _defined_values(use_define(platform))
        ):
            continue
        add_define(use_define(platform))
        if (shared := _SHARED_DEFINES.get(platform)) is not None:
            add_define(shared)
        _add_count_define(
            f"ESPHOME_ENTITY_{platform.upper()}_COUNT", len(objs), platform
        )
        if reserved:
            # slot_counter emits this one; only check that nothing else defines it differently
            _check_count_define(runtime_count_define(platform), reserved, platform)
        else:
            _add_count_define(runtime_count_define(platform), 0, platform)
        _emit_list(upper, objs)
    for upper in ("DEVICE", "AREA"):
        if (objs := state.lists.get(upper)) is not None:
            _add_count_define(f"ESPHOME_{upper}_COUNT", len(objs), upper.lower())
            _emit_list(upper, objs)


def schedule_emit() -> None:
    """Schedule the FINAL job that emits the lists; core's to_code calls it once per run."""
    CORE.add_job(_emit_entity_lists)
