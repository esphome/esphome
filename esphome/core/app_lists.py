"""Constant App lists of entities, devices and areas, emitted by codegen."""

from dataclasses import dataclass, field

from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.core.entity_types import DATETIME_SUBTYPES, ENTITY_PLATFORMS
from esphome.cpp_generator import (
    MockObj,
    RawStatement,
    add_define,
    add_global,
    static_pointer_address,
)

DOMAIN = "app_lists"


def _upper(platform: str) -> str:
    upper = platform.upper()
    return f"DATETIME_{upper}" if platform in DATETIME_SUBTYPES else upper


ENTITY_LISTS: dict[str, str] = {
    platform: _upper(platform) for platform in ENTITY_PLATFORMS
}


def use_define(platform: str) -> str:
    return f"USE_{_upper(platform)}"


@dataclass
class AppListsState:
    lists: dict[str, list[str]] = field(default_factory=dict)
    emitted: bool = False


def _get_state() -> AppListsState:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = AppListsState()
    return CORE.data[DOMAIN]


def _check_not_emitted(what: str) -> AppListsState:
    if (state := _get_state()).emitted:
        raise EsphomeError(
            f"{what} after the lists were emitted; do it from to_code, not a FINAL job"
        )
    return state


def add_to_list(upper: str, var: MockObj) -> None:
    state = _check_not_emitted(f"{var} was added to the {upper} list")
    if (address := static_pointer_address(var)) is None:
        raise EsphomeError(
            f"{var} must be created with cg.new_Pvariable to be added to the {upper} list; "
            "entities cannot be added at run time"
        )
    state.lists.setdefault(upper, []).append(address)


def add_entity(platform: str, var: MockObj) -> None:
    add_to_list(ENTITY_LISTS[platform], var)


def _defined_values() -> dict[str, set[str]]:
    # Lazy: esphome.platformio imports codegen
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
    if others := sorted(defined.get(name, set()) - {str(value)}):
        raise EsphomeError(
            f"{name} is defined as {', '.join(others)} elsewhere, but codegen sizes it as "
            f"{value}. Remove that define; {reason}."
        )
    add_define(name, value)


def _emit_list(upper: str, objs: list[str], count_define: str) -> None:
    name = f"esphome::app_lists::APP_{upper}_LIST"
    # An empty list still needs a definition; it is never read
    items = ", ".join(objs) or "nullptr"
    add_global(RawStatement(f"decltype({name}) {name} PROGMEM = {{{items}}};"))
    # Also catches a count defined after the check above
    add_global(
        RawStatement(
            f'static_assert({count_define} == {len(objs)}, "{count_define} must match the {upper} list");'
        )
    )


@coroutine_with_priority(CoroPriority.FINAL)
async def _emit_entity_lists() -> None:
    state = _get_state()
    state.emitted = True
    defined = _defined_values()
    for platform, upper in ENTITY_LISTS.items():
        objs = state.lists.get(upper, [])
        # Decides USE_* and the list together; a USE_* defined directly counts as enabled
        if (
            not CORE.platform_counts.get(platform)
            and use_define(platform) not in defined
        ):
            continue
        add_define(use_define(platform))
        if platform in ("infrared", "radio_frequency"):
            add_define("USE_IR_RF")
        count_define = f"ESPHOME_ENTITY_{platform.upper()}_COUNT"
        _add_count_define(defined, count_define, len(objs))
        _emit_list(upper, objs, count_define)
    for upper in ("DEVICE", "AREA"):
        if (objs := state.lists.get(upper)) is not None:
            count_define = f"ESPHOME_{upper}_COUNT"
            _add_count_define(
                defined,
                count_define,
                len(objs),
                "devices and areas come from the YAML devices: and areas: lists",
            )
            _emit_list(upper, objs, count_define)


def schedule_emit() -> None:
    CORE.add_job(_emit_entity_lists)
