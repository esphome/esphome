"""
Functions and classes to translate the `restore_mode:` and `restore_state:` config keys into
the single runtime state callback that `LightState` actually understands.
"""

from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_COLOR_MODE, CONF_STATE
from esphome.core import Lambda
from esphome.cpp_generator import call_lambda
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

from .automation import LIGHT_STATE_FIELDS, validate_light_state
from .types import ColorMode, LightStateRTCState

RESTORE_STATE_KEEP = "KEEP"
RESTORE_STATE_INVERT = "INVERT"
RESTORE_STATE_INITIAL = "INITIAL"
RESTORE_STATE_ALL = "ALL"
RESTORE_STATE_NONE = "NONE"


@dataclass(frozen=True)
class LegacyRestoreMode:
    cold_boot_state: bool
    restore_action: bool | str | None  # None = no override, "INVERT", or force-to-bool
    save_enabled: bool


LEGACY_RESTORE_MODES: dict[str, LegacyRestoreMode] = {
    "RESTORE_DEFAULT_OFF": LegacyRestoreMode(False, None, True),
    "RESTORE_DEFAULT_ON": LegacyRestoreMode(True, None, True),
    "ALWAYS_OFF": LegacyRestoreMode(False, None, False),
    "ALWAYS_ON": LegacyRestoreMode(True, None, False),
    "RESTORE_INVERTED_DEFAULT_OFF": LegacyRestoreMode(
        False, RESTORE_STATE_INVERT, True
    ),
    "RESTORE_INVERTED_DEFAULT_ON": LegacyRestoreMode(True, RESTORE_STATE_INVERT, True),
    "RESTORE_AND_OFF": LegacyRestoreMode(False, False, True),
    "RESTORE_AND_ON": LegacyRestoreMode(True, True, True),
}

# (config key, LightStateRTCState member) for every field, and for every field but `state`.
_ALL_STATE_FIELDS: tuple[tuple[str, str], ...] = tuple(
    (field.conf_key, field.member) for field in LIGHT_STATE_FIELDS
)
_STATE_STRUCT_FIELDS: tuple[tuple[str, str], ...] = tuple(
    (conf_key, member)
    for conf_key, member in _ALL_STATE_FIELDS
    if conf_key != CONF_STATE
)
# Canonical struct-member order, used only to make generated code deterministic --
# these are independent field assignments, so the actual order never affects behavior.
_MEMBER_ORDER: tuple[str, ...] = tuple(member for _, member in _ALL_STATE_FIELDS)

# A pending `s.<member> = <value>;` statement, tagged with the member it writes.
StateStatement = tuple[str, str]


def _inferred_color_mode(initial_state_config: ConfigType | None) -> str | None:
    """A C++ expression for the capabilities needed by the colour fields `initial_state:`
    sets, or None if `color_mode:` is given or no colour field is set.

    Colour modes are bitmasks of capabilities, so the result is a bare capability set,
    not necessarily a real mode; LightState::setup() resolves it to a mode the light
    actually supports.
    """
    if not initial_state_config or CONF_COLOR_MODE in initial_state_config:
        return None
    modes = sorted(
        {
            str(field.color_mode)
            for field in LIGHT_STATE_FIELDS
            if field.color_mode is not None
            and initial_state_config.get(field.conf_key) is not None
        }
    )
    if not modes:
        return None
    mask = " | ".join(f"static_cast<uint8_t>({mode})" for mode in modes)
    return f"static_cast<{ColorMode}>({mask})"


def _partition_state_statements(
    initial_statements: list[StateStatement],
    restore_statements: list[StateStatement],
    save_enabled: bool,
) -> list[str]:
    """Split initial/restore statements into what must run unconditionally versus what
    depends on `restored`, and render the resulting lambda body lines.

    Fields whose statement is identical in both branches (e.g. RESTORE_AND_ON's
    cold-boot and restore-time statements are both "s.state = true;") are hoisted out
    of the `restored` branch entirely, so only the fields that actually depend on
    `restored` end up inside it -- down to no branch at all when every field overlaps.
    A member appearing more than once in the same list keeps only its last statement
    (a plain, side-effect-free assignment): matches sequential-execution semantics,
    since an earlier write to the same member is always fully overwritten by a later
    one in the original code this replaces.

    `save_enabled` is false exactly when `restore_statements` is empty and `restored`
    is unconditionally false at the call site (nothing is ever loaded), so the
    initial-only branch can skip its `if (!restored)` guard entirely.
    """
    # dict() over (member, statement) pairs keeps the *last* entry per member.
    initial_map = dict(initial_statements)
    restore_map = dict(restore_statements)
    # A member outside _MEMBER_ORDER would be silently skipped below instead of
    # raising -- catch that here so a typo doesn't turn into wrong state on a device.
    assert set(initial_map) <= set(_MEMBER_ORDER)
    assert set(restore_map) <= set(_MEMBER_ORDER)

    common: list[str] = []
    only_initial: list[str] = []
    only_restore: list[str] = []
    for member in _MEMBER_ORDER:
        initial_stmt = initial_map.get(member)
        restore_stmt = restore_map.get(member)
        if initial_stmt is not None and initial_stmt == restore_stmt:
            common.append(initial_stmt)
            continue
        if initial_stmt is not None:
            only_initial.append(initial_stmt)
        if restore_stmt is not None:
            only_restore.append(restore_stmt)

    body = common
    if only_restore and only_initial:
        body += ["if (restored) {", *only_restore, "} else {", *only_initial, "}"]
    elif only_restore:
        body += ["if (restored) {", *only_restore, "}"]
    elif only_initial:
        if save_enabled:
            body += ["if (!restored) {", *only_initial, "}"]
        else:
            body += only_initial
    return body


async def _build_state_lambda(
    initial_statements: list[StateStatement],
    restore_statements: list[StateStatement],
    save_enabled: bool,
) -> Lambda | None:
    """
    Combine the initial and restore statements into a single lambda that applies the
    correct values to a `LightStateRTCState &s` depending on whether a persisted state
    actually loaded.
    """
    if not initial_statements and not restore_statements:
        return None
    body = _partition_state_statements(
        initial_statements, restore_statements, save_enabled
    )
    args = [(LightStateRTCState.operator("ref"), "s"), (cg.bool_, "restored")]
    return await cg.process_lambda(Lambda("\n".join(body)), args, return_type=cg.void)


async def _process_value(value: Any, member: str) -> str:
    if isinstance(value, Lambda):
        return_type = cg.bool_ if member == CONF_STATE else cg.float_
        lamb = await cg.process_lambda(value, [], return_type=return_type)
        return call_lambda(lamb)
    return cg.safe_exp(value)


async def _initial_state_statements(
    initial_state_config: ConfigType | None,
) -> list[StateStatement]:
    """
    Create assignments for every field the user set in `initial_state:`, in canonical
    struct-member order. A field given as `!lambda` is resolved and called immediately,
    the same way `light.control`'s own field lambdas are.
    """
    if not initial_state_config:
        return []
    statements: list[StateStatement] = []
    for conf_key, member in _ALL_STATE_FIELDS:
        if (value := initial_state_config.get(conf_key)) is None:
            continue
        statements.append(
            (member, f"s.{member} = {await _process_value(value, member)};")
        )
    if (inferred := _inferred_color_mode(initial_state_config)) is not None:
        statements.append(("color_mode", f"s.color_mode = {inferred};"))
    return statements


async def _resolve_initial_value(
    conf_key: str, member: str, initial_state_config: ConfigType | None
) -> str:
    """
    Return the C++ expression to use for a `restore_state:` field whose value is INITIAL
    """
    if (
        initial_state_config is not None
        and (value := initial_state_config.get(conf_key)) is not None
    ):
        return await _process_value(value, member)
    if conf_key == CONF_COLOR_MODE and (
        inferred := _inferred_color_mode(initial_state_config)
    ):
        return inferred
    return f"LightStateRTCState{{}}.{member}"


async def _restore_state_statements(
    restore_state_config: ConfigType, initial_state_config: ConfigType | None
) -> list[StateStatement]:
    """
    Create a list of statements to apply the user's `restore_state:` config
    """
    statements: list[StateStatement] = []
    state = restore_state_config[CONF_STATE]
    if state == RESTORE_STATE_INVERT:
        statements.append(("state", "s.state = !s.state;"))
    elif state == RESTORE_STATE_INITIAL:
        expr = await _resolve_initial_value(CONF_STATE, "state", initial_state_config)
        statements.append(("state", f"s.state = {expr};"))
    elif state != RESTORE_STATE_KEEP:
        statements.append(("state", f"s.state = {cg.safe_exp(state)};"))
    for conf_key, member in _STATE_STRUCT_FIELDS:
        value = restore_state_config[conf_key]
        if value == RESTORE_STATE_INITIAL:
            expr = await _resolve_initial_value(conf_key, member, initial_state_config)
        elif value == RESTORE_STATE_KEEP:
            continue
        else:
            expr = cg.safe_exp(value)
        statements.append((member, f"s.{member} = {expr};"))
    return statements


def _legacy_restore_statements(mode: LegacyRestoreMode) -> list[StateStatement]:
    """
    Create a list of statements to apply the legacy restore_mode: behavior.
    """
    if mode.restore_action is None:
        return []
    if mode.restore_action == RESTORE_STATE_INVERT:
        return [("state", "s.state = !s.state;")]
    return [("state", f"s.state = {str(mode.restore_action).lower()};")]


def _legacy_cold_boot_statements(
    mode: LegacyRestoreMode, initial_state_config: ConfigType | None
) -> list[StateStatement]:
    """
    Create a list of statements to apply the legacy restore_mode: cold-boot behavior.
    """
    existing_state = (
        initial_state_config.get(CONF_STATE) if initial_state_config else None
    )
    if existing_state is None:
        if not mode.cold_boot_state:
            return []  # already matches LightStateRTCState's own default
    elif existing_state == mode.cold_boot_state:
        return []  # initial_state: already set exactly this value
    return [("state", f"s.state = {str(mode.cold_boot_state).lower()};")]


def _initial_state_overridden_by_legacy_mode(
    mode: LegacyRestoreMode, initial_state_config: ConfigType | None
) -> bool:
    """
    Is the user-config `initial_state:` value for `state` overridden by the legacy mode?
    """
    if initial_state_config is None or CONF_STATE not in initial_state_config:
        return False
    return initial_state_config[CONF_STATE] != mode.cold_boot_state


def _keep_or(validator: Callable[[Any], Any]) -> Callable[[Any], Any]:
    """
    Extend a validator to also accept the `KEEP`/`INITIAL` options
    """

    @schema_extractor("one_of")
    def validate(value: Any) -> Any:
        if value == SCHEMA_EXTRACT:
            # Editor completion: the sentinels plus the wrapped validator's own values,
            # if it is an enum. Numeric validators have none to offer.
            try:
                inner = tuple(validator(SCHEMA_EXTRACT))
            except cv.Invalid:
                inner = ()
            return (RESTORE_STATE_KEEP, RESTORE_STATE_INITIAL, *inner)
        if isinstance(value, str):
            upper = value.strip().upper()
            if upper in (RESTORE_STATE_KEEP, RESTORE_STATE_INITIAL):
                return upper
        return validator(value)

    return validate


@schema_extractor("one_of")
def _validate_restore_state_state(value: Any) -> str | bool:
    if value == SCHEMA_EXTRACT:
        return (
            RESTORE_STATE_KEEP,
            RESTORE_STATE_INVERT,
            RESTORE_STATE_INITIAL,
            *validate_light_state(SCHEMA_EXTRACT),
        )
    if isinstance(value, str):
        upper = value.strip().upper()
        if upper in (RESTORE_STATE_KEEP, RESTORE_STATE_INVERT, RESTORE_STATE_INITIAL):
            return upper
    return validate_light_state(value)


_RESTORE_STATE_FIELDS_SCHEMA = cv.Schema(
    {
        cv.Optional(field.conf_key, default=RESTORE_STATE_KEEP): (
            _validate_restore_state_state
            if field.conf_key == CONF_STATE
            else _keep_or(field.validator)
        )
        for field in LIGHT_STATE_FIELDS
    }
)


@schema_extractor("schema")
def RESTORE_STATE_SCHEMA(value: Any) -> ConfigType | str:
    """
    The restore_state: config key can be a mapping of per-field overrides, `all`, or `none`.
    """
    if value == SCHEMA_EXTRACT:
        # The `all`/`none` string shorthands have no representation in the extracted
        # docs schema; only the per-field mapping form is walked here.
        return _RESTORE_STATE_FIELDS_SCHEMA
    if isinstance(value, str):
        upper = value.strip().upper()
        if upper == RESTORE_STATE_ALL:
            value = {}
        elif upper == RESTORE_STATE_NONE:
            return RESTORE_STATE_NONE
    return _RESTORE_STATE_FIELDS_SCHEMA(value)
