"""Tests for esphome.automation module."""

from collections.abc import Generator
from unittest.mock import patch

import pytest

from esphome.automation import has_deferred_actions
from esphome.util import RegistryEntry


def _make_registry(deferred_actions: set[str]) -> dict[str, RegistryEntry]:
    """Create a mock ACTION_REGISTRY with specified deferred actions."""
    registry: dict[str, RegistryEntry] = {}
    for name in deferred_actions:
        registry[name] = RegistryEntry(name, lambda: None, None, None, deferred=True)
    return registry


@pytest.fixture
def mock_registry() -> Generator[dict[str, RegistryEntry]]:
    """Fixture that patches ACTION_REGISTRY with delay, wait_until, script.wait as deferred."""
    registry: dict[str, RegistryEntry] = _make_registry(
        {"delay", "wait_until", "script.wait"}
    )
    registry["logger.log"] = RegistryEntry(
        "logger.log", lambda: None, None, None, deferred=False
    )
    with patch("esphome.automation.ACTION_REGISTRY", registry):
        yield registry


def test_has_deferred_actions_empty_list(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_deferred_actions([]) is False


def test_has_deferred_actions_empty_dict(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_deferred_actions({}) is False


def test_has_deferred_actions_non_dict_non_list(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_deferred_actions("string") is False
    assert has_deferred_actions(42) is False
    assert has_deferred_actions(None) is False


def test_has_deferred_actions_delay(mock_registry: dict[str, RegistryEntry]) -> None:
    assert has_deferred_actions([{"delay": "1s"}]) is True


def test_has_deferred_actions_wait_until(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_deferred_actions([{"wait_until": {"condition": {}}}]) is True


def test_has_deferred_actions_script_wait(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_deferred_actions([{"script.wait": "script_id"}]) is True


def test_has_deferred_actions_non_deferred(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert has_deferred_actions([{"logger.log": "hello"}]) is False


def test_has_deferred_actions_unknown(mock_registry: dict[str, RegistryEntry]) -> None:
    assert has_deferred_actions([{"unknown.action": "value"}]) is False


def test_has_deferred_actions_nested_in_then(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """Deferred action nested inside a non-deferred action's then block."""
    actions: list[dict[str, object]] = [
        {
            "logger.log": "first",
            "then": [{"delay": "1s"}],
        }
    ]
    assert has_deferred_actions(actions) is True


def test_has_deferred_actions_deeply_nested(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """Deferred action deeply nested in action structure."""
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
    assert has_deferred_actions(actions) is True


def test_has_deferred_actions_no_deferred_in_nested(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """No deferred actions even with nesting."""
    actions: list[dict[str, object]] = [
        {
            "if": {
                "then": [
                    {"logger.log": "hello"},
                ]
            }
        }
    ]
    assert has_deferred_actions(actions) is False


def test_has_deferred_actions_multiple_one_deferred(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert (
        has_deferred_actions(
            [
                {"logger.log": "first"},
                {"delay": "1s"},
                {"logger.log": "second"},
            ]
        )
        is True
    )


def test_has_deferred_actions_multiple_none_deferred(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    assert (
        has_deferred_actions(
            [
                {"logger.log": "first"},
                {"logger.log": "second"},
            ]
        )
        is False
    )


def test_has_deferred_actions_dict_input(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """Direct dict input (single action)."""
    assert has_deferred_actions({"delay": "1s"}) is True
    assert has_deferred_actions({"logger.log": "hello"}) is False
