"""
Functions and classes to translate the `restore_mode:` and `restore_state:` config keys into
the single runtime state callback that `LightState` actually understands.
"""

from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_BRIGHTNESS,
    CONF_COLD_WHITE,
    CONF_COLOR_BRIGHTNESS,
    CONF_COLOR_MODE,
    CONF_COLOR_TEMPERATURE,
    CONF_GREEN,
    CONF_RED,
    CONF_STATE,
    CONF_WARM_WHITE,
    CONF_WHITE,
)
from esphome.core import Lambda
from esphome.types import ConfigType

from .automation import validate_light_state
from .types import COLOR_MODES, LightStateRTCState

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

# Config key -> LightStateRTCState member, for every field other than `state`
# (config key differs from the struct member only for color_temperature -> color_temp).
_STATE_STRUCT_FIELDS: tuple[tuple[str, str], ...] = (
    (CONF_COLOR_MODE, "color_mode"),
    (CONF_BRIGHTNESS, "brightness"),
    (CONF_COLOR_BRIGHTNESS, "color_brightness"),
    (CONF_RED, "red"),
    (CONF_GREEN, "green"),
    (CONF_BLUE, "blue"),
    (CONF_WHITE, "white"),
    (CONF_COLOR_TEMPERATURE, "color_temp"),
    (CONF_COLD_WHITE, "cold_white"),
    (CONF_WARM_WHITE, "warm_white"),
)
_ALL_STATE_FIELDS: tuple[tuple[str, str], ...] = (
    (CONF_STATE, "state"),
    *_STATE_STRUCT_FIELDS,
)
# Canonical struct-member order, used only to make generated code deterministic --
# these are independent field assignments, so the actual order never affects behavior.
_MEMBER_ORDER: tuple[str, ...] = tuple(member for _, member in _ALL_STATE_FIELDS)

# A pending `s.<member> = <value>;` statement, tagged with the member it writes.
StateStatement = tuple[str, str]


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


def _initial_state_statements(
    initial_state_config: ConfigType | None,
) -> list[StateStatement]:
    """
    Create assignments for every field the user set in `initial_state:`, in canonical struct-member order.
    """
    if not initial_state_config:
        return []
    return [
        (member, f"s.{member} = {cg.safe_exp(value)};")
        for conf_key, member in _ALL_STATE_FIELDS
        if (value := initial_state_config.get(conf_key)) is not None
    ]


def _resolve_initial_value(
    conf_key: str, member: str, initial_state_config: ConfigType | None
) -> str:
    """
    Return the C++ expression to use for a `restore_state:` field whose value is INITIAL
    """
    if (
        initial_state_config is not None
        and (value := initial_state_config.get(conf_key)) is not None
    ):
        return cg.safe_exp(value)
    return f"LightStateRTCState{{}}.{member}"


def _restore_state_statements(
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
        expr = _resolve_initial_value(CONF_STATE, "state", initial_state_config)
        statements.append(("state", f"s.state = {expr};"))
    elif state != RESTORE_STATE_KEEP:
        statements.append(("state", f"s.state = {cg.safe_exp(state)};"))
    for conf_key, member in _STATE_STRUCT_FIELDS:
        value = restore_state_config[conf_key]
        if value == RESTORE_STATE_INITIAL:
            expr = _resolve_initial_value(conf_key, member, initial_state_config)
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

    def validate(value: Any) -> Any:
        if isinstance(value, str):
            upper = value.strip().upper()
            if upper in (RESTORE_STATE_KEEP, RESTORE_STATE_INITIAL):
                return upper
        return validator(value)

    return validate


def _validate_restore_state_state(value: Any) -> str | bool:
    if isinstance(value, str):
        upper = value.strip().upper()
        if upper in (RESTORE_STATE_KEEP, RESTORE_STATE_INVERT, RESTORE_STATE_INITIAL):
            return upper
    return validate_light_state(value)


_RESTORE_STATE_FIELDS_SCHEMA = cv.Schema(
    {
        cv.Optional(
            CONF_STATE, default=RESTORE_STATE_KEEP
        ): _validate_restore_state_state,
        cv.Optional(CONF_COLOR_MODE, default=RESTORE_STATE_KEEP): _keep_or(
            cv.enum(COLOR_MODES, upper=True, space="_")
        ),
        cv.Optional(CONF_BRIGHTNESS, default=RESTORE_STATE_KEEP): _keep_or(
            cv.percentage
        ),
        cv.Optional(CONF_COLOR_BRIGHTNESS, default=RESTORE_STATE_KEEP): _keep_or(
            cv.percentage
        ),
        cv.Optional(CONF_RED, default=RESTORE_STATE_KEEP): _keep_or(cv.percentage),
        cv.Optional(CONF_GREEN, default=RESTORE_STATE_KEEP): _keep_or(cv.percentage),
        cv.Optional(CONF_BLUE, default=RESTORE_STATE_KEEP): _keep_or(cv.percentage),
        cv.Optional(CONF_WHITE, default=RESTORE_STATE_KEEP): _keep_or(cv.percentage),
        cv.Optional(CONF_COLOR_TEMPERATURE, default=RESTORE_STATE_KEEP): _keep_or(
            cv.color_temperature
        ),
        cv.Optional(CONF_COLD_WHITE, default=RESTORE_STATE_KEEP): _keep_or(
            cv.percentage
        ),
        cv.Optional(CONF_WARM_WHITE, default=RESTORE_STATE_KEEP): _keep_or(
            cv.percentage
        ),
    }
)


def RESTORE_STATE_SCHEMA(value: Any) -> ConfigType | str:
    """
    The restore_state: config key can be a mapping of per-field overrides, `all`, or `none`.
    """
    if isinstance(value, str):
        upper = value.strip().upper()
        if upper == RESTORE_STATE_ALL:
            value = {}
        elif upper == RESTORE_STATE_NONE:
            return RESTORE_STATE_NONE
    return _RESTORE_STATE_FIELDS_SCHEMA(value)
