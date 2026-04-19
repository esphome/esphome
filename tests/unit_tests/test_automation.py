"""Tests for esphome.automation module."""

from collections.abc import Generator
from unittest.mock import AsyncMock, call, patch

import pytest

from esphome.automation import (
    CallbackAutomation,
    TriggerForwarder,
    TriggerOnFalseForwarder,
    TriggerOnTrueForwarder,
    build_callback_automations,
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


@pytest.fixture
def mock_build_callback() -> Generator[AsyncMock]:
    """Patch build_callback_automation to capture calls."""
    with patch(
        "esphome.automation.build_callback_automation", new_callable=AsyncMock
    ) as mock:
        yield mock


@pytest.mark.asyncio
async def test_build_callback_automations_empty_entries(
    mock_build_callback: AsyncMock,
) -> None:
    """No entries means no calls."""
    parent = MockObj("var", "->")
    await build_callback_automations(parent, {}, ())
    mock_build_callback.assert_not_called()


@pytest.mark.asyncio
async def test_build_callback_automations_missing_config_key(
    mock_build_callback: AsyncMock,
) -> None:
    """Entry present but config key missing -- no calls."""
    parent = MockObj("var", "->")
    await build_callback_automations(
        parent,
        {},
        (CallbackAutomation("on_state", "add_on_state_callback", [(bool, "x")]),),
    )
    mock_build_callback.assert_not_called()


@pytest.mark.asyncio
async def test_build_callback_automations_single_entry(
    mock_build_callback: AsyncMock,
) -> None:
    """Single entry with one config triggers one call."""
    parent = MockObj("var", "->")
    conf: dict[str, object] = {"automation_id": "auto_1", "then": []}
    config: dict[str, list[dict[str, object]]] = {"on_state": [conf]}
    await build_callback_automations(
        parent,
        config,
        (CallbackAutomation("on_state", "add_on_state_callback", [(bool, "x")]),),
    )
    mock_build_callback.assert_called_once_with(
        parent, "add_on_state_callback", [(bool, "x")], conf, forwarder=None
    )


@pytest.mark.asyncio
async def test_build_callback_automations_multiple_configs(
    mock_build_callback: AsyncMock,
) -> None:
    """Single entry with multiple configs triggers multiple calls."""
    parent = MockObj("var", "->")
    conf1: dict[str, object] = {"automation_id": "auto_1", "then": []}
    conf2: dict[str, object] = {"automation_id": "auto_2", "then": []}
    config: dict[str, list[dict[str, object]]] = {"on_state": [conf1, conf2]}
    await build_callback_automations(
        parent,
        config,
        (CallbackAutomation("on_state", "add_on_state_callback", [(bool, "x")]),),
    )
    assert mock_build_callback.call_count == 2
    mock_build_callback.assert_any_call(
        parent, "add_on_state_callback", [(bool, "x")], conf1, forwarder=None
    )
    mock_build_callback.assert_any_call(
        parent, "add_on_state_callback", [(bool, "x")], conf2, forwarder=None
    )


@pytest.mark.asyncio
async def test_build_callback_automations_multiple_entries(
    mock_build_callback: AsyncMock,
) -> None:
    """Multiple entries each with one config."""
    parent = MockObj("var", "->")
    conf_a: dict[str, object] = {"automation_id": "auto_a", "then": []}
    conf_b: dict[str, object] = {"automation_id": "auto_b", "then": []}
    config: dict[str, list[dict[str, object]]] = {
        "on_value": [conf_a],
        "on_raw_value": [conf_b],
    }
    await build_callback_automations(
        parent,
        config,
        (
            CallbackAutomation("on_value", "add_on_value_callback", [(float, "x")]),
            CallbackAutomation(
                "on_raw_value", "add_on_raw_value_callback", [(float, "x")]
            ),
        ),
    )
    assert mock_build_callback.call_count == 2
    assert mock_build_callback.call_args_list == [
        call(parent, "add_on_value_callback", [(float, "x")], conf_a, forwarder=None),
        call(
            parent, "add_on_raw_value_callback", [(float, "x")], conf_b, forwarder=None
        ),
    ]


@pytest.mark.asyncio
async def test_build_callback_automations_with_forwarder(
    mock_build_callback: AsyncMock,
) -> None:
    """Entry with forwarder passes it through."""
    parent = MockObj("var", "->")
    conf: dict[str, object] = {"automation_id": "auto_1", "then": []}
    config: dict[str, list[dict[str, object]]] = {"on_press": [conf]}
    await build_callback_automations(
        parent,
        config,
        (
            CallbackAutomation(
                "on_press", "add_on_state_callback", forwarder=TriggerOnTrueForwarder
            ),
        ),
    )
    mock_build_callback.assert_called_once_with(
        parent, "add_on_state_callback", [], conf, forwarder=TriggerOnTrueForwarder
    )


@pytest.mark.asyncio
async def test_build_callback_automations_mixed_entries(
    mock_build_callback: AsyncMock,
) -> None:
    """Mix of entries with args, forwarders, and defaults."""
    parent = MockObj("var", "->")
    conf_state: dict[str, object] = {"automation_id": "auto_1", "then": []}
    conf_press: dict[str, object] = {"automation_id": "auto_2", "then": []}
    conf_release: dict[str, object] = {"automation_id": "auto_3", "then": []}
    config: dict[str, list[dict[str, object]]] = {
        "on_state": [conf_state],
        "on_press": [conf_press],
        "on_release": [conf_release],
    }
    await build_callback_automations(
        parent,
        config,
        (
            CallbackAutomation("on_state", "add_on_state_callback", [(bool, "x")]),
            CallbackAutomation(
                "on_press", "add_on_state_callback", forwarder=TriggerOnTrueForwarder
            ),
            CallbackAutomation(
                "on_release", "add_on_state_callback", forwarder=TriggerOnFalseForwarder
            ),
        ),
    )
    assert mock_build_callback.call_count == 3
    assert mock_build_callback.call_args_list == [
        call(
            parent, "add_on_state_callback", [(bool, "x")], conf_state, forwarder=None
        ),
        call(
            parent,
            "add_on_state_callback",
            [],
            conf_press,
            forwarder=TriggerOnTrueForwarder,
        ),
        call(
            parent,
            "add_on_state_callback",
            [],
            conf_release,
            forwarder=TriggerOnFalseForwarder,
        ),
    ]


@pytest.mark.asyncio
async def test_build_callback_automations_skips_missing_keys(
    mock_build_callback: AsyncMock,
) -> None:
    """Entries whose config keys are absent are silently skipped."""
    parent = MockObj("var", "->")
    conf: dict[str, object] = {"automation_id": "auto_1", "then": []}
    config: dict[str, list[dict[str, object]]] = {"on_press": [conf]}
    await build_callback_automations(
        parent,
        config,
        (
            CallbackAutomation(
                "on_press", "add_on_state_callback", forwarder=TriggerOnTrueForwarder
            ),
            CallbackAutomation(
                "on_release", "add_on_state_callback", forwarder=TriggerOnFalseForwarder
            ),
        ),
    )
    mock_build_callback.assert_called_once_with(
        parent, "add_on_state_callback", [], conf, forwarder=TriggerOnTrueForwarder
    )


@pytest.mark.asyncio
async def test_build_callback_automations_defaults(
    mock_build_callback: AsyncMock,
) -> None:
    """Verify CallbackAutomation with only required fields defaults args=[] and forwarder=None."""
    parent = MockObj("var", "->")
    conf: dict[str, object] = {"automation_id": "auto_1", "then": []}
    config: dict[str, list[dict[str, object]]] = {"on_press": [conf]}
    await build_callback_automations(
        parent,
        config,
        (CallbackAutomation("on_press", "add_on_press_callback"),),
    )
    mock_build_callback.assert_called_once_with(
        parent, "add_on_press_callback", [], conf, forwarder=None
    )
