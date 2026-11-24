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
            f"{var} must be created with cg.new_Pvariable to be added to the {upper} list"
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
    request_slot = slot_counter(runtime_count_define(platform))
    for _ in range(count):
        request_slot()


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
        reserved = get_slot_count(runtime_count_define(platform))
        # The one place that decides whether the type is compiled in, so USE_* and the list agree
        if not reserved and not CORE.platform_counts.get(platform):
            continue
        add_define(use_define(platform))
        add_define(f"ESPHOME_ENTITY_{platform.upper()}_COUNT", len(objs))
        if not reserved:
            add_define(runtime_count_define(platform), 0)
        _emit_list(upper, objs)
    for upper in ("DEVICE", "AREA"):
        if (objs := state.lists.get(upper)) is not None:
            add_define(f"ESPHOME_{upper}_COUNT", len(objs))
            _emit_list(upper, objs)


def schedule_emit() -> None:
    """Schedule the FINAL job that emits the lists; core's to_code calls it once per run."""
    CORE.add_job(_emit_entity_lists)
