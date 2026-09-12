"""Tests for the restore_mode/restore_state -> runtime lambda translation layer."""

import pytest

import esphome.codegen as cg
from esphome.components.light import (
    CONF_RESTORE_MODE,
    CONF_RESTORE_STATE,
    LIGHT_SCHEMA,
    LightType,
    light_schema,
)
from esphome.components.light.restore_state import (
    LEGACY_RESTORE_MODES,
    RESTORE_STATE_KEEP,
    RESTORE_STATE_NONE,
    RESTORE_STATE_SCHEMA,
    StateStatement,
    _initial_state_overridden_by_legacy_mode,
    _legacy_cold_boot_statements,
    _partition_state_statements,
    _restore_state_statements,
)
import esphome.config_validation as cv
from esphome.const import CONF_STATE
from esphome.core import Lambda

# (mode name, expected cold_boot_state, expected restore_action, expected save_enabled)
LEGACY_MODE_TABLE = [
    ("RESTORE_DEFAULT_OFF", False, None, True),
    ("RESTORE_DEFAULT_ON", True, None, True),
    ("ALWAYS_OFF", False, None, False),
    ("ALWAYS_ON", True, None, False),
    ("RESTORE_INVERTED_DEFAULT_OFF", False, "INVERT", True),
    ("RESTORE_INVERTED_DEFAULT_ON", True, "INVERT", True),
    ("RESTORE_AND_OFF", False, False, True),
    ("RESTORE_AND_ON", True, True, True),
]


@pytest.mark.parametrize(
    ("mode", "cold_boot_state", "restore_action", "save_enabled"), LEGACY_MODE_TABLE
)
def test_legacy_restore_mode_translation(
    mode: str, cold_boot_state: bool, restore_action, save_enabled: bool
) -> None:
    legacy = LEGACY_RESTORE_MODES[mode]
    assert legacy.cold_boot_state is cold_boot_state
    assert legacy.restore_action == restore_action
    assert legacy.save_enabled is save_enabled


def test_all_eight_legacy_modes_present() -> None:
    assert set(LEGACY_RESTORE_MODES) == {mode for mode, *_ in LEGACY_MODE_TABLE}


def test_restore_mode_and_restore_state_are_exclusive() -> None:
    with pytest.raises(cv.Invalid, match="restore"):
        LIGHT_SCHEMA(
            {
                "name": "test",
                CONF_RESTORE_MODE: "ALWAYS_ON",
                CONF_RESTORE_STATE: {},
            }
        )


def test_neither_restore_key_required_or_defaulted() -> None:
    config = LIGHT_SCHEMA({"name": "test"})
    assert CONF_RESTORE_MODE not in config
    assert CONF_RESTORE_STATE not in config


def test_restore_state_empty_config_keeps_everything() -> None:
    config = RESTORE_STATE_SCHEMA({})
    assert all(value == RESTORE_STATE_KEEP for value in config.values())


@pytest.mark.parametrize("value", ["all", "All", "ALL"])
def test_restore_state_all_shorthand_is_case_insensitive(value: str) -> None:
    assert RESTORE_STATE_SCHEMA(value) == RESTORE_STATE_SCHEMA({})


def test_restore_state_rejects_other_strings() -> None:
    with pytest.raises(cv.Invalid):
        RESTORE_STATE_SCHEMA("everything")


_DummyLight = cg.esphome_ns.class_("DummyLight")


def test_default_restore_mode_applies_when_neither_key_given() -> None:
    schema = light_schema(
        _DummyLight, LightType.BINARY, default_restore_mode="RESTORE_DEFAULT_ON"
    )
    config = schema({"id": "light1"})
    assert config[CONF_RESTORE_MODE] == "RESTORE_DEFAULT_ON"
    assert CONF_RESTORE_STATE not in config


def test_default_restore_mode_is_dropped_when_restore_state_given() -> None:
    schema = light_schema(
        _DummyLight, LightType.BINARY, default_restore_mode="RESTORE_DEFAULT_ON"
    )
    config = schema({"id": "light1", "restore_state": {"state": "INVERT"}})
    assert CONF_RESTORE_MODE not in config
    assert config[CONF_RESTORE_STATE][CONF_STATE] == "INVERT"


def test_default_restore_mode_still_exclusive_with_explicit_restore_state() -> None:
    schema = light_schema(
        _DummyLight, LightType.BINARY, default_restore_mode="RESTORE_DEFAULT_ON"
    )
    with pytest.raises(cv.Invalid, match="restore"):
        schema(
            {
                "id": "light1",
                CONF_RESTORE_MODE: "ALWAYS_ON",
                CONF_RESTORE_STATE: {},
            }
        )


def test_default_restore_mode_result_still_extendable() -> None:
    # light_schema() must keep returning a real cv.Schema (not e.g. cv.All) even when
    # default_restore_mode is given, since every in-tree light platform chains
    # .extend() on its result.
    schema = light_schema(
        _DummyLight, LightType.BINARY, default_restore_mode="RESTORE_DEFAULT_ON"
    )
    extended = schema.extend({})
    config = extended({"id": "light1"})
    assert config[CONF_RESTORE_MODE] == "RESTORE_DEFAULT_ON"


@pytest.mark.parametrize("value", ["none", "None", "NONE"])
def test_restore_state_none_shorthand_is_case_insensitive(value: str) -> None:
    assert RESTORE_STATE_SCHEMA(value) == RESTORE_STATE_NONE


def test_restore_state_none_is_still_exclusive_with_restore_mode() -> None:
    # cv.Exclusive checks which keys are present, regardless of their resolved
    # value, so restore_state: none must still conflict with restore_mode:.
    with pytest.raises(cv.Invalid, match="restore"):
        LIGHT_SCHEMA(
            {
                "name": "test",
                CONF_RESTORE_MODE: "ALWAYS_ON",
                CONF_RESTORE_STATE: "none",
            }
        )


def test_restore_state_explicit_overrides_leave_others_keep() -> None:
    config = RESTORE_STATE_SCHEMA({"state": "invert", "brightness": "100%"})
    assert config["state"] == "INVERT"
    assert config["brightness"] == pytest.approx(1.0)
    assert config["color_mode"] == RESTORE_STATE_KEEP
    assert config["red"] == RESTORE_STATE_KEEP


@pytest.mark.parametrize("value", ["keep", "Keep", "KEEP"])
def test_restore_state_state_accepts_keep_case_insensitively(value: str) -> None:
    assert RESTORE_STATE_SCHEMA({"state": value})["state"] == RESTORE_STATE_KEEP


@pytest.mark.parametrize("value", ["invert", "Invert", "INVERT"])
def test_restore_state_state_accepts_invert_case_insensitively(value: str) -> None:
    assert RESTORE_STATE_SCHEMA({"state": value})["state"] == "INVERT"


@pytest.mark.parametrize("value", ["initial", "Initial", "INITIAL"])
def test_restore_state_state_accepts_initial_case_insensitively(value: str) -> None:
    assert RESTORE_STATE_SCHEMA({"state": value})["state"] == "INITIAL"


@pytest.mark.parametrize("value", ["initial", "Initial", "INITIAL"])
def test_restore_state_other_fields_accept_initial_case_insensitively(
    value: str,
) -> None:
    assert RESTORE_STATE_SCHEMA({"brightness": value})["brightness"] == "INITIAL"


@pytest.mark.parametrize(
    ("value", "expected"), [("ON", True), ("OFF", False), (True, True), (False, False)]
)
def test_restore_state_state_prioritizes_on_off(
    value: str | bool, expected: bool
) -> None:
    # A quoted "ON"/"OFF" string, distinct from KEEP/INVERT, still resolves via
    # validate_light_state -- matching initial_state:'s own state field.
    assert RESTORE_STATE_SCHEMA({"state": value})["state"] is expected


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("state", Lambda("return true;")),
        ("brightness", Lambda("return 1.0;")),
        ("color_mode", Lambda("return light::ColorMode::ON_OFF;")),
    ],
)
def test_restore_state_fields_reject_lambda(field: str, value: Lambda) -> None:
    with pytest.raises(cv.Invalid):
        RESTORE_STATE_SCHEMA({field: value})


@pytest.mark.parametrize(
    ("mode", "initial_state_config", "expected"),
    [
        # No initial_state:, cold_boot_state False -- already matches
        # LightStateRTCState's own `state{false}` default, nothing to emit.
        ("ALWAYS_OFF", None, []),
        ("ALWAYS_OFF", {}, []),
        # No initial_state:, cold_boot_state True -- differs from the default.
        ("ALWAYS_ON", None, [("state", "s.state = true;")]),
        # initial_state: already set exactly the cold-boot value -- redundant.
        ("ALWAYS_OFF", {CONF_STATE: False}, []),
        ("RESTORE_AND_ON", {CONF_STATE: True}, []),
        # initial_state: set a different value -- must be overridden.
        ("ALWAYS_OFF", {CONF_STATE: True}, [("state", "s.state = false;")]),
        ("RESTORE_AND_ON", {CONF_STATE: False}, [("state", "s.state = true;")]),
    ],
)
def test_legacy_cold_boot_statements_skips_redundant_defaults(
    mode: str, initial_state_config: dict | None, expected: list[StateStatement]
) -> None:
    legacy = LEGACY_RESTORE_MODES[mode]
    assert _legacy_cold_boot_statements(legacy, initial_state_config) == expected


def test_partition_no_overlap_produces_full_if_else() -> None:
    body = _partition_state_statements(
        [("brightness", "s.brightness = 1.0f;")],
        [("state", "s.state = false;")],
        True,
    )
    assert body == [
        "if (restored) {",
        "s.state = false;",
        "} else {",
        "s.brightness = 1.0f;",
        "}",
    ]


def test_partition_full_overlap_drops_branch_entirely() -> None:
    # Both branches want exactly the same thing (e.g. RESTORE_AND_ON): no `if` at all.
    body = _partition_state_statements(
        [("state", "s.state = true;")],
        [("state", "s.state = true;")],
        True,
    )
    assert body == ["s.state = true;"]


def test_partition_partial_overlap_hoists_shared_field() -> None:
    # `state` matches in both branches and is hoisted out; `red`/`color_mode` differ
    # per branch and stay inside a single-sided `if` each.
    body = _partition_state_statements(
        [
            ("red", "s.red = 0.5f;"),
            ("state", "s.state = true;"),
        ],
        [
            ("state", "s.state = true;"),
            ("color_mode", "s.color_mode = light::ColorMode::ON_OFF;"),
        ],
        True,
    )
    assert body == [
        "s.state = true;",
        "if (restored) {",
        "s.color_mode = light::ColorMode::ON_OFF;",
        "} else {",
        "s.red = 0.5f;",
        "}",
    ]


def test_partition_duplicate_member_in_one_list_keeps_last_write() -> None:
    # initial_statements can legitimately contain two writes to `state`: the user's own
    # initial_state: value, followed by a legacy mode's cold-boot override. The earlier
    # one is dead code (immediately overwritten) and must not survive partitioning.
    body = _partition_state_statements(
        [
            ("state", "s.state = true;"),
            ("state", "s.state = false;"),
        ],
        [("state", "s.state = false;")],
        True,
    )
    assert body == ["s.state = false;"]


def test_partition_initial_only_without_save_skips_guard() -> None:
    # save_enabled False means restored is unconditionally false at the call site
    # (e.g. ALWAYS_OFF, restore_state: none, or neither key configured), so guarding
    # the initial-only statements behind `if (!restored)` would only waste flash.
    body = _partition_state_statements(
        [("brightness", "s.brightness = 1.0f;")],
        [],
        False,
    )
    assert body == ["s.brightness = 1.0f;"]


@pytest.mark.parametrize(
    ("mode", "initial_state_config", "expected"),
    [
        # No initial_state: at all -- nothing to override.
        ("ALWAYS_OFF", None, False),
        ("ALWAYS_ON", None, False),
        # initial_state: set, but doesn't include state -- nothing to override.
        ("ALWAYS_OFF", {}, False),
        # initial_state: state already matches the mode's cold-boot value -- no-op.
        ("ALWAYS_OFF", {CONF_STATE: False}, False),
        ("RESTORE_AND_ON", {CONF_STATE: True}, False),
        # initial_state: state set to something the mode's cold-boot force overrides.
        ("ALWAYS_OFF", {CONF_STATE: True}, True),
        ("RESTORE_AND_ON", {CONF_STATE: False}, True),
    ],
)
def test_initial_state_overridden_by_legacy_mode(
    mode: str, initial_state_config: dict | None, expected: bool
) -> None:
    legacy = LEGACY_RESTORE_MODES[mode]
    assert (
        _initial_state_overridden_by_legacy_mode(legacy, initial_state_config)
        == expected
    )


def test_restore_state_initial_state_field_copies_initial_state_value() -> None:
    restore_state_config = RESTORE_STATE_SCHEMA({"state": "initial"})
    statements = _restore_state_statements(restore_state_config, {CONF_STATE: True})
    assert statements == [("state", "s.state = true;")]


def test_restore_state_initial_other_field_copies_initial_state_value() -> None:
    restore_state_config = RESTORE_STATE_SCHEMA({"brightness": "initial"})
    initial_state_config = {"brightness": 0.5}
    statements = _restore_state_statements(restore_state_config, initial_state_config)
    assert statements == [("brightness", "s.brightness = 0.5f;")]


def test_restore_state_initial_falls_back_to_struct_default() -> None:
    # No initial_state: at all -- INITIAL resolves to a read of LightStateRTCState's
    # own member-initializer default, straight from the struct.
    restore_state_config = RESTORE_STATE_SCHEMA({"brightness": "initial"})
    statements = _restore_state_statements(restore_state_config, None)
    assert statements == [
        ("brightness", "s.brightness = LightStateRTCState{}.brightness;")
    ]


def test_restore_state_initial_falls_back_when_initial_state_omits_field() -> None:
    restore_state_config = RESTORE_STATE_SCHEMA({"brightness": "initial"})
    initial_state_config = {"state": True}  # doesn't set brightness
    statements = _restore_state_statements(restore_state_config, initial_state_config)
    assert statements == [
        ("brightness", "s.brightness = LightStateRTCState{}.brightness;")
    ]
