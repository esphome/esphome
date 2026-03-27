"""Tests for esphome.automation module."""

from collections.abc import Generator
from unittest.mock import patch

import pytest

from esphome.automation import (
    TriggerForwarder,
    TriggerOnFalseForwarder,
    TriggerOnTrueForwarder,
    has_non_synchronous_actions,
)
from esphome.cpp_generator import MockObj, RawExpression
from esphome.util import RegistryEntry


def _make_registry(non_synchronous_actions: set[str]) -> dict[str, RegistryEntry]:
    """Create a mock ACTION_REGISTRY with specified non-synchronous actions.

    Uses the default synchronous=False, matching the real registry behavior.
    """
    registry: dict[str, RegistryEntry] = {}
    for name in non_synchronous_actions:
        registry[name] = RegistryEntry(name, lambda: None, None, None)
    return registry


@pytest.fixture
def mock_registry() -> Generator[dict[str, RegistryEntry]]:
    """Fixture that patches ACTION_REGISTRY with delay, wait_until, script.wait as non-synchronous."""
    registry: dict[str, RegistryEntry] = _make_registry(
        {"delay", "wait_until", "script.wait"}
    )
    registry["logger.log"] = RegistryEntry(
        "logger.log", lambda: None, None, None, synchronous=True
    )
    with patch("esphome.automation.ACTION_REGISTRY", registry):
        yield registry


def test_has_non_synchronous_actions_empty_list(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_non_synchronous_actions([]) is False


def test_has_non_synchronous_actions_empty_dict(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_non_synchronous_actions({}) is False


def test_has_non_synchronous_actions_non_dict_non_list(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_non_synchronous_actions("string") is False
    assert has_non_synchronous_actions(42) is False
    assert has_non_synchronous_actions(None) is False


def test_has_non_synchronous_actions_delay(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_non_synchronous_actions([{"delay": "1s"}]) is True


def test_has_non_synchronous_actions_wait_until(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_non_synchronous_actions([{"wait_until": {"condition": {}}}]) is True


def test_has_non_synchronous_actions_script_wait(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_non_synchronous_actions([{"script.wait": "script_id"}]) is True


def test_has_non_synchronous_actions_synchronous(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_non_synchronous_actions([{"logger.log": "hello"}]) is False


def test_has_non_synchronous_actions_unknown_not_in_registry(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """Unknown actions not in registry are not flagged (only registered actions count)."""
    assert has_non_synchronous_actions([{"unknown.action": "value"}]) is False


def test_has_non_synchronous_actions_default_non_synchronous(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """Actions registered without explicit synchronous=True default to non-synchronous."""
    mock_registry["some.action"] = RegistryEntry(
        "some.action", lambda: None, None, None
    )
    assert has_non_synchronous_actions([{"some.action": "value"}]) is True


def test_has_non_synchronous_actions_nested_in_then(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """Non-synchronous action nested inside a synchronous action's then block."""
    actions: list[dict[str, object]] = [
        {
            "logger.log": "first",
            "then": [{"delay": "1s"}],
        }
    ]
    assert has_non_synchronous_actions(actions) is True


def test_has_non_synchronous_actions_deeply_nested(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """Non-synchronous action deeply nested in action structure."""
    actions: list[dict[str, object]] = [
        {
            "if": {
                "then": [
                    {"logger.log": "hello"},
                    {"delay": "500ms"},
                ]
            }
        }
    ]
    assert has_non_synchronous_actions(actions) is True


def test_has_non_synchronous_actions_none_in_nested(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """No non-synchronous actions even with nesting."""
    actions: list[dict[str, object]] = [
        {
            "if": {
                "then": [
                    {"logger.log": "hello"},
                ]
            }
        }
    ]
    assert has_non_synchronous_actions(actions) is False


def test_has_non_synchronous_actions_multiple_one_non_synchronous(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert (
        has_non_synchronous_actions(
            [
                {"logger.log": "first"},
                {"delay": "1s"},
                {"logger.log": "second"},
            ]
        )
        is True
    )


def test_has_non_synchronous_actions_multiple_all_synchronous(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert (
        has_non_synchronous_actions(
            [
                {"logger.log": "first"},
                {"logger.log": "second"},
            ]
        )
        is False
    )


def test_has_non_synchronous_actions_dict_input(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """Direct dict input (single action)."""
    assert has_non_synchronous_actions({"delay": "1s"}) is True
    assert has_non_synchronous_actions({"logger.log": "hello"}) is False


def _build_forwarder(
    automation_name: str,
    args: list[tuple[str, str]],
    forwarder: MockObj | None = None,
) -> str:
    """Build a trigger forwarder expression the same way build_callback_automation does.

    Mirrors the forwarder selection logic in automation.build_callback_automation.
    """
    import esphome.codegen as cg

    obj = MockObj(automation_name, "->")
    if forwarder is None:
        arg_types = [RawExpression(t) for t, _ in args]
        templ = (
            cg.TemplateArguments(*arg_types) if arg_types else cg.TemplateArguments()
        )
        forwarder = TriggerForwarder.template(templ)
    return f"{forwarder}{{{obj}}}"


def test_trigger_forwarder_no_args() -> None:
    """Button on_press: TriggerForwarder<> with no args."""
    result = _build_forwarder("auto_1", [])
    assert result == "TriggerForwarder<>{auto_1}"


def test_trigger_forwarder_single_float_arg() -> None:
    """Sensor on_value: TriggerForwarder<float>."""
    result = _build_forwarder("auto_1", [("float", "x")])
    assert result == "TriggerForwarder<float>{auto_1}"


def test_trigger_forwarder_single_bool_arg() -> None:
    """Switch on_state: TriggerForwarder<bool>."""
    result = _build_forwarder("auto_1", [("bool", "x")])
    assert result == "TriggerForwarder<bool>{auto_1}"


def test_trigger_forwarder_on_true() -> None:
    """Binary_sensor on_press / switch on_turn_on: TriggerOnTrueForwarder."""
    result = _build_forwarder("auto_1", [], forwarder=TriggerOnTrueForwarder)
    assert result == "TriggerOnTrueForwarder{auto_1}"


def test_trigger_forwarder_on_false() -> None:
    """Binary_sensor on_release / switch on_turn_off: TriggerOnFalseForwarder."""
    result = _build_forwarder("auto_1", [], forwarder=TriggerOnFalseForwarder)
    assert result == "TriggerOnFalseForwarder{auto_1}"


def test_trigger_forwarder_multiple_args() -> None:
    """Binary_sensor on_state_change: TriggerForwarder with two args."""
    result = _build_forwarder(
        "auto_1",
        [("optional<bool>", "x_previous"), ("optional<bool>", "x")],
    )
    assert result == "TriggerForwarder<optional<bool>, optional<bool>>{auto_1}"


def test_trigger_forwarder_string_arg() -> None:
    """Text_sensor on_value: TriggerForwarder<std::string>."""
    result = _build_forwarder("auto_1", [("std::string", "x")])
    assert result == "TriggerForwarder<std::string>{auto_1}"


def test_trigger_forwarder_custom_type() -> None:
    """Custom forwarder type passed directly."""
    custom = MockObj("MyForwarder", "")
    result = _build_forwarder("auto_1", [], forwarder=custom)
    assert result == "MyForwarder{auto_1}"
