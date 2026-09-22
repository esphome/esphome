"""Tests for esphome.automation module."""

from collections.abc import Callable, Generator
from functools import partial
from typing import NamedTuple
from unittest.mock import AsyncMock, MagicMock, call, patch

import pytest

from esphome.automation import (
    ApplyAction,
    ApplyCall,
    ApplyField,
    CallbackAutomation,
    TriggerForwarder,
    TriggerOnFalseForwarder,
    TriggerOnTrueForwarder,
    build_callback_automations,
    has_non_synchronous_actions,
    register_apply_action,
    register_bare_action,
    register_bare_condition,
    register_parented_action,
    register_parented_condition,
    register_simple_action,
    register_simple_condition,
)
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID, Lambda
from esphome.cpp_generator import MockObj, RawExpression
from esphome.util import Registry, RegistryEntry


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


PARENT_ID = ID("my_component")
PARENT_OBJ = MockObj("parent", "->")
NEW_OBJ = MockObj("var", "->")
ACTION_TYPE = cg.esphome_ns.class_("MyAction")
CONDITION_TYPE = cg.esphome_ns.class_("MyCondition")
TEMPLATE_ARG = cg.TemplateArguments()


class MockCodegen(NamedTuple):
    get_variable: AsyncMock
    new_pvariable: MagicMock
    register_parented: AsyncMock


@pytest.fixture
def mock_cg() -> Generator[MockCodegen]:
    """Patch the codegen calls the shared builders make."""
    with (
        patch("esphome.codegen.get_variable", new_callable=AsyncMock) as get_variable,
        patch("esphome.codegen.new_Pvariable") as new_pvariable,
        patch(
            "esphome.codegen.register_parented", new_callable=AsyncMock
        ) as register_parented,
    ):
        get_variable.return_value = PARENT_OBJ
        new_pvariable.return_value = NEW_OBJ
        yield MockCodegen(get_variable, new_pvariable, register_parented)


@pytest.fixture
def registries() -> Generator[tuple[Registry, Registry]]:
    """Patch both registries so registrations made by a test do not leak."""
    actions = Registry()
    conditions = Registry()
    with (
        patch("esphome.automation.ACTION_REGISTRY", actions),
        patch("esphome.automation.CONDITION_REGISTRY", conditions),
    ):
        yield actions, conditions


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("register", "is_action", "ctor_parent", "parented"),
    [
        (partial(register_simple_action, synchronous=True), True, True, False),
        (partial(register_bare_action, synchronous=True), True, False, False),
        (partial(register_parented_action, synchronous=True), True, False, True),
        (register_simple_condition, False, True, False),
        (register_bare_condition, False, False, False),
        (register_parented_condition, False, False, True),
    ],
    ids=[
        "simple_action",
        "bare_action",
        "parented_action",
        "simple_condition",
        "bare_condition",
        "parented_condition",
    ],
)
async def test_shared_builders(
    registries: tuple[Registry, Registry],
    mock_cg: MockCodegen,
    register: Callable[..., None],
    is_action: bool,
    ctor_parent: bool,
    parented: bool,
) -> None:
    """Each helper constructs the object and wires the parent the way its C++ shape needs."""
    actions, conditions = registries
    type_id = ACTION_TYPE if is_action else CONDITION_TYPE
    register("my.entry", type_id, {})
    entry = (actions if is_action else conditions)["my.entry"]
    assert entry.type_id is type_id
    config = {CONF_ID: PARENT_ID} if ctor_parent or parented else {}

    result = await entry.fun(config, ID("obj_1"), TEMPLATE_ARG, [])

    assert result is NEW_OBJ
    if ctor_parent:
        mock_cg.get_variable.assert_awaited_once_with(PARENT_ID)
        mock_cg.new_pvariable.assert_called_once_with(
            ID("obj_1"), TEMPLATE_ARG, PARENT_OBJ
        )
    else:
        mock_cg.get_variable.assert_not_called()
        mock_cg.new_pvariable.assert_called_once_with(ID("obj_1"), TEMPLATE_ARG)
    if parented:
        mock_cg.register_parented.assert_awaited_once_with(NEW_OBJ, PARENT_ID)
    else:
        mock_cg.register_parented.assert_not_called()


@pytest.mark.parametrize("synchronous", [True, False])
def test_shared_builders_keep_synchronous_flag(
    registries: tuple[Registry, Registry], synchronous: bool
) -> None:
    """The synchronous flag reaches the registry entry unchanged."""
    actions, _ = registries
    register_simple_action("my.simple", ACTION_TYPE, {}, synchronous=synchronous)
    register_bare_action("my.bare", ACTION_TYPE, {}, synchronous=synchronous)
    register_parented_action("my.parented", ACTION_TYPE, {}, synchronous=synchronous)
    assert actions["my.simple"].synchronous is synchronous
    assert actions["my.bare"].synchronous is synchronous
    assert actions["my.parented"].synchronous is synchronous


FAN_DIRECTION = cg.esphome_ns.enum("FanDirection", is_class=True)


async def _run_apply_action(
    registries: tuple[Registry, Registry],
    fields: tuple[ApplyField | ApplyCall, ...],
    config: dict[str, object],
    args: list[tuple[object, str]] | None = None,
    call: str | None = None,
) -> RegistryEntry:
    """Register an apply action and run its builder with the given config."""
    actions, _ = registries
    register_apply_action("my.apply", {}, *fields, call=call)
    entry = actions["my.apply"]
    args = args or []
    template_arg = cg.TemplateArguments(*(t for t, _ in args))
    await entry.fun({CONF_ID: PARENT_ID, **config}, ID("obj_1"), template_arg, args)
    return entry


def _apply_lambda(mock_cg: MockCodegen) -> str:
    return str(mock_cg.new_pvariable.call_args.args[2])


@pytest.mark.asyncio
async def test_register_apply_action_entry(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """The action is the core ApplyAction, synchronous, built from one lambda with const-ref args."""
    entry = await _run_apply_action(
        registries, (), {}, args=[(cg.int32, "x"), (cg.bool_, "y")]
    )
    assert entry.type_id is ApplyAction
    assert entry.synchronous is True
    mock_cg.get_variable.assert_awaited_once_with(PARENT_ID)
    action_id, template_arg, apply_lambda = mock_cg.new_pvariable.call_args.args
    assert action_id == ID("obj_1")
    assert str(template_arg) == "<int32_t, bool>"
    assert str(apply_lambda).startswith(
        "[](const std::remove_cvref_t<int32_t> & x, "
        "const std::remove_cvref_t<bool> & y) -> void {"
    )


@pytest.mark.asyncio
async def test_apply_constants(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """Constants are immediates, strings stay in flash, absent keys emit nothing, order is kept."""
    direction = cv.enum({"FORWARD": FAN_DIRECTION.FORWARD})("FORWARD")
    fields = (
        ApplyField("kp", "set_kp", cg.float_),
        ApplyField("ki", "set_ki", cg.float_),
        ApplyField("on", "set_on", cg.bool_),
        ApplyField("direction", "set_direction", FAN_DIRECTION),
        ApplyField("song", "play", cg.std_string),
        ApplyField("position", "position = {}", cg.float_),
        ApplyCall("publish_state()"),
    )
    config = {
        "kp": 0.0,
        "on": False,
        "direction": direction,
        "song": "a:b",
        "position": 0.5,
    }
    await _run_apply_action(registries, fields, config)
    text = _apply_lambda(mock_cg)
    lines = [
        f"{PARENT_OBJ}->set_kp(0.0f);",
        f"{PARENT_OBJ}->set_on(false);",
        f"{PARENT_OBJ}->set_direction({FAN_DIRECTION.FORWARD});",
        f'{PARENT_OBJ}->play(progmem_string(ESPHOME_F("a:b")));',
        f"{PARENT_OBJ}->position = 0.5f;",
        f"{PARENT_OBJ}->publish_state();",
    ]
    positions = [text.index(line) for line in lines]
    assert positions == sorted(positions)
    assert "set_ki" not in text


@pytest.mark.asyncio
async def test_apply_lambdas(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """A single return reduces to a cast, anything longer is called inline with the trigger args."""
    fields = (
        ApplyField("kp", "set_kp", cg.float_),
        ApplyField("ki", "set_ki", cg.float_),
    )
    config = {
        "kp": Lambda("return x * 2;"),
        "ki": Lambda("if (x) return 1.0f;\nreturn 2.0f;"),
    }
    await _run_apply_action(registries, fields, config, args=[(cg.int32, "x")])
    text = _apply_lambda(mock_cg)
    assert f"{PARENT_OBJ}->set_kp(static_cast<float>(x * 2));" in text
    # Outer apply lambda and inner field lambda spell the trigger arg identically.
    assert text.count("const std::remove_cvref_t<int32_t> & x") == 2
    assert (
        f"{PARENT_OBJ}->set_ki([](const std::remove_cvref_t<int32_t> & x) -> float {{"
        in text
    )
    assert "}(x));" in text


@pytest.mark.asyncio
async def test_apply_call_needs_every_key(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    fields = (
        ApplyCall("set_range({}, {})", (("low", cg.float_), ("high", cg.float_))),
    )
    await _run_apply_action(registries, fields, {"low": 1.0, "high": 2.0})
    assert f"{PARENT_OBJ}->set_range(1.0f, 2.0f);" in _apply_lambda(mock_cg)

    mock_cg.new_pvariable.reset_mock()
    await _run_apply_action(registries, fields, {"low": 1.0})
    assert "set_range" not in _apply_lambda(mock_cg)


@pytest.mark.asyncio
async def test_apply_action_call_shape(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    fields = (ApplyField("brightness", "set_brightness", cg.float_),)
    await _run_apply_action(registries, fields, {"brightness": 0.5}, call="make_call")
    text = _apply_lambda(mock_cg)
    lines = [
        f"auto call = {PARENT_OBJ}->make_call();",
        "call.set_brightness(0.5f);",
        "call.perform();",
    ]
    positions = [text.index(line) for line in lines]
    assert positions == sorted(positions)


@pytest.mark.asyncio
async def test_apply_field_nested_key_const_fn_and_type_fn(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    fields = (
        ApplyField(("vertical", "direction"), "set_direction", cg.int_),
        ApplyField(
            "name",
            "set_name",
            cg.std_string,
            const_fn=lambda config, value: f"{cg.safe_exp(value)}, {len(value)}",
        ),
        ApplyField(
            "value",
            "value() = {}",
            type_fn=lambda parent: cg.RawExpression(f"decltype({parent}->value())"),
        ),
    )
    config = {
        "vertical": {"direction": 3},
        "name": "abc",
        "value": Lambda("return 42;"),
    }
    await _run_apply_action(registries, fields, config)
    text = _apply_lambda(mock_cg)
    assert f"{PARENT_OBJ}->set_direction(3);" in text
    assert f'{PARENT_OBJ}->set_name("abc", 3);' in text
    assert (
        f"{PARENT_OBJ}->value() = static_cast<decltype({PARENT_OBJ}->value())>(42);"
        in text
    )

    mock_cg.new_pvariable.reset_mock()
    await _run_apply_action(registries, fields[:1], {"vertical": {}})
    assert "set_direction" not in _apply_lambda(mock_cg)


def test_apply_registration_checks(registries: tuple[Registry, Registry]) -> None:
    with pytest.raises(ValueError, match="2 placeholder"):
        register_apply_action(
            "my.apply", {}, ApplyCall("set_range({}, {})", (("low", cg.float_),))
        )
    with pytest.raises(ValueError, match="exactly one of type_ and type_fn"):
        register_apply_action("my.apply", {}, ApplyField("kp", "set_kp"))
    schema = cv.Schema({cv.Required(CONF_ID): cv.string, cv.Optional("kp"): cv.float_})
    register_apply_action("my.ok", schema, ApplyField("kp", "set_kp", cg.float_))
    with pytest.raises(ValueError, match="'kd' is not in the schema"):
        register_apply_action("my.bad", schema, ApplyField("kd", "set_kd", cg.float_))
