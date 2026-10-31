"""Tests for esphome.automation module."""

from collections.abc import Callable, Generator
from functools import partial
from typing import NamedTuple
from unittest.mock import AsyncMock, MagicMock, call, patch

import pytest

from esphome.automation import (
    _COLLECTED_ACTION,
    ACTION_REGISTRY,
    CONF_CHOICE_ID,
    CONF_CHOOSE,
    IN_RANGE_CHECK,
    IN_RANGE_DEFAULTS,
    Action,
    AndCondition,
    ApplyAction,
    ApplyBytes,
    ApplyCall,
    ApplyCondition,
    ApplyField,
    Automation,
    CallbackAutomation,
    DelayAction,
    ForCondition,
    IfAction,
    LambdaAction,
    LambdaCondition,
    NotCondition,
    OrCondition,
    RepeatAction,
    ResumeComponentAction,
    StatelessLambdaCondition,
    TriggerForwarder,
    TriggerOnFalseForwarder,
    TriggerOnTrueForwarder,
    WaitUntilAction,
    WhileAction,
    XorCondition,
    _shadow_iteration,
    apply_action_builder,
    apply_statements_action,
    build_action,
    build_action_list,
    build_callback_automation,
    build_callback_automations,
    build_condition,
    build_parent_callback_automation,
    build_trigger_automations,
    build_trigger_callback,
    choose_action_to_code,
    has_non_synchronous_actions,
    if_action_to_code,
    literal_with_length,
    maybe_simple_id,
    new_lambda_pvariable,
    parent_ref,
    progmem_bytes,
    register_action,
    register_apply_action,
    register_apply_condition,
    register_bare_action,
    register_bare_condition,
    register_condition,
    register_parented_action,
    register_parented_condition,
    register_simple_action,
    register_simple_condition,
    string_ref_literal,
    templatable_bytes,
    validate_action,
    validate_automation,
    validate_no_deferring_actions,
    validate_potentially_or_condition,
    validate_synchronous_automation,
)
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALL,
    CONF_ANY,
    CONF_AUTOMATION_ID,
    CONF_CONDITION,
    CONF_COUNT,
    CONF_ELSE,
    CONF_ID,
    CONF_THEN,
    CONF_TIME,
    CONF_TIMEOUT,
    CONF_TYPE_ID,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import CORE, ID, KEY_CORE, KEY_TARGET_PLATFORM, EsphomeError, Lambda
from esphome.cpp_generator import LambdaExpression, MockObj, RawExpression
from esphome.schema_extractors import SCHEMA_EXTRACT
from esphome.types import ConfigType
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


def test_has_non_synchronous_actions_option_named_like_action(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """An option that shares a name with a deferring action is not that action."""
    actions = [{"logger.log": {"message": "hello", "delay": "1s"}}]
    assert has_non_synchronous_actions(actions) is False


def test_has_non_synchronous_actions_nested_option_named_like_action(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """Option keys in nested option dicts are not looked up as action names either."""
    actions = [{"logger.log": {"settings": {"wait_until": {"condition": {}}}}}]
    assert has_non_synchronous_actions(actions) is False


def test_has_non_synchronous_actions_option_named_like_action_inside_then(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    actions = [
        {
            "if": {
                "condition": {},
                "then": [{"logger.log": {"message": "hello", "delay": "1s"}}],
            }
        }
    ]
    assert has_non_synchronous_actions(actions) is False


def test_has_non_synchronous_actions_in_else(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """Action lists in any option of an action are searched, not only then:."""
    actions = [
        {
            "if": {
                "condition": {},
                "then": [{"logger.log": "hello"}],
                "else": [{"delay": "1s"}],
            }
        }
    ]
    assert has_non_synchronous_actions(actions) is True


def test_has_non_synchronous_actions_automation_dict(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """A single automation, as validate_automation(single=True) returns it."""
    assert has_non_synchronous_actions({CONF_THEN: [{"delay": "1s"}]}) is True
    assert has_non_synchronous_actions({CONF_THEN: [{"logger.log": "hi"}]}) is False


def test_has_non_synchronous_actions_set_playlist_delay() -> None:
    """speaker_source.set_playlist_delay is synchronous despite its delay: option."""
    import esphome.components.speaker_source.media_player  # noqa: F401  # registers the action

    actions = [
        {
            "speaker_source.set_playlist_delay": {
                CONF_ID: "player",
                "pipeline": 0,
                "delay": 1000,
            }
        }
    ]
    assert has_non_synchronous_actions(actions) is False


def test_validate_no_deferring_actions_accepts_synchronous(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    value = [{CONF_THEN: [{"logger.log": "hello"}]}]
    assert validate_no_deferring_actions("on_test")(value) is value


def test_validate_no_deferring_actions_accepts_lambda_form(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    """A returning lambda has no action list, so there is nothing to reject."""
    value = Lambda("return true;")
    assert validate_no_deferring_actions("on_test")(value) is value


def test_validate_no_deferring_actions_rejects_deferring(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    validator = validate_no_deferring_actions("on_test", "the test data")
    with pytest.raises(
        cv.Invalid,
        match=r"not allowed in on_test: the test data is only valid while",
    ):
        validator([{CONF_THEN: [{"logger.log": "hello"}, {"delay": "1s"}]}])


def test_validate_no_deferring_actions_default_data(
    mock_registry: dict[str, RegistryEntry],
) -> None:
    with pytest.raises(cv.Invalid, match=r"on_test: the trigger data is only valid"):
        validate_no_deferring_actions("on_test")([{CONF_THEN: [{"delay": "1s"}]}])


def test_validate_synchronous_automation_accepts_synchronous() -> None:
    result = validate_synchronous_automation("on_test")(
        [{CONF_THEN: [{"lambda": Lambda('ESP_LOGD("test", "ran");')}]}]
    )
    assert len(result) == 1


def test_validate_synchronous_automation_rejects_deferring() -> None:
    with pytest.raises(cv.Invalid, match="Deferring actions"):
        validate_synchronous_automation("on_test")([{CONF_THEN: [{"delay": "1s"}]}])


def test_validate_synchronous_automation_rejects_nested_deferring() -> None:
    actions = [
        {
            "if": {
                "condition": {"lambda": Lambda("return true;")},
                CONF_THEN: [{"delay": "1s"}],
            }
        }
    ]
    with pytest.raises(cv.Invalid, match="Deferring actions"):
        validate_synchronous_automation("on_test")([{CONF_THEN: actions}])


def test_validate_synchronous_automation_passes_options() -> None:
    """extra_schema and single are passed through to validate_automation()."""
    validator = validate_synchronous_automation(
        "on_test", extra_schema={cv.Optional("retry"): cv.string}, single=True
    )
    result = validator(
        {"retry": "yes", CONF_THEN: [{"lambda": Lambda('ESP_LOGD("test", "ran");')}]}
    )
    assert isinstance(result, dict)
    assert result["retry"] == "yes"
    with pytest.raises(cv.Invalid, match="Deferring actions"):
        validator({CONF_THEN: [{"delay": "1s"}]})


def test_validate_synchronous_automation_passes_extra_validators() -> None:
    """extra_validators is passed through to validate_automation() and runs on each automation."""
    seen: list[ConfigType] = []

    def extra_validator(value: ConfigType) -> ConfigType:
        seen.append(value)
        return value

    validator = validate_synchronous_automation(
        "on_test", extra_validators=extra_validator
    )
    validator([{CONF_THEN: [{"lambda": Lambda('ESP_LOGD("test", "ran");')}]}])
    assert len(seen) == 1
    assert CONF_THEN in seen[0]


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
        parent,
        "add_on_state_callback",
        [(bool, "x")],
        conf,
        forwarder=None,
        params=None,
        forward=None,
        when=None,
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
        parent,
        "add_on_state_callback",
        [(bool, "x")],
        conf1,
        forwarder=None,
        params=None,
        forward=None,
        when=None,
    )
    mock_build_callback.assert_any_call(
        parent,
        "add_on_state_callback",
        [(bool, "x")],
        conf2,
        forwarder=None,
        params=None,
        forward=None,
        when=None,
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
        call(
            parent,
            "add_on_value_callback",
            [(float, "x")],
            conf_a,
            forwarder=None,
            params=None,
            forward=None,
            when=None,
        ),
        call(
            parent,
            "add_on_raw_value_callback",
            [(float, "x")],
            conf_b,
            forwarder=None,
            params=None,
            forward=None,
            when=None,
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
        parent,
        "add_on_state_callback",
        [],
        conf,
        forwarder=TriggerOnTrueForwarder,
        params=None,
        forward=None,
        when=None,
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
            parent,
            "add_on_state_callback",
            [(bool, "x")],
            conf_state,
            forwarder=None,
            params=None,
            forward=None,
            when=None,
        ),
        call(
            parent,
            "add_on_state_callback",
            [],
            conf_press,
            forwarder=TriggerOnTrueForwarder,
            params=None,
            forward=None,
            when=None,
        ),
        call(
            parent,
            "add_on_state_callback",
            [],
            conf_release,
            forwarder=TriggerOnFalseForwarder,
            params=None,
            forward=None,
            when=None,
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
        parent,
        "add_on_state_callback",
        [],
        conf,
        forwarder=TriggerOnTrueForwarder,
        params=None,
        forward=None,
        when=None,
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
        parent,
        "add_on_press_callback",
        [],
        conf,
        forwarder=None,
        params=None,
        forward=None,
        when=None,
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
    add_global: MagicMock
    add_global_after_includes: MagicMock
    add: MagicMock
    calls: MagicMock  # the global emitters and new_pvariable attached, to check their order


@pytest.fixture
def mock_build_automation() -> Generator[AsyncMock]:
    with patch("esphome.automation.build_automation", new_callable=AsyncMock) as mock:
        yield mock


@pytest.mark.asyncio
async def test_build_trigger_automations_with_parent(
    mock_build_automation: AsyncMock,
) -> None:
    """Each entry's Trigger class is instantiated with the parent and built with its args."""
    parent = MockObj("var", "->")
    on_conf = {"trigger_id": ID("trig_1"), "then": []}
    set_conf = {"trigger_id": ID("trig_2"), "then": []}
    config = {"on_turn_on": [on_conf], "on_speed_set": [set_conf]}
    with patch("esphome.codegen.new_Pvariable") as new_pvariable:
        new_pvariable.side_effect = lambda id_, *args: MockObj(str(id_), "->")
        await build_trigger_automations(
            parent,
            config,
            (
                ("on_turn_on", []),
                ("on_turn_off", []),
                ("on_speed_set", [(cg.int_, "x")]),
            ),
        )
    assert [c.args for c in new_pvariable.call_args_list] == [
        (ID("trig_1"), parent),
        (ID("trig_2"), parent),
    ]
    calls = mock_build_automation.call_args_list
    assert [(str(c.args[0]), c.args[1], c.args[2]) for c in calls] == [
        ("trig_1", [], on_conf),
        ("trig_2", [(cg.int_, "x")], set_conf),
    ]


@pytest.mark.asyncio
async def test_build_trigger_automations_without_parent(
    mock_build_automation: AsyncMock,
) -> None:
    """A None parent instantiates the Trigger class with no constructor arguments."""
    conf = {"trigger_id": ID("trig_1"), "then": []}
    with patch("esphome.codegen.new_Pvariable") as new_pvariable:
        await build_trigger_automations(None, {"on_boot": [conf]}, (("on_boot", []),))
    new_pvariable.assert_called_once_with(ID("trig_1"))
    mock_build_automation.assert_awaited_once()


@pytest.fixture
def mock_cg() -> Generator[MockCodegen]:
    """Patch the codegen calls the shared builders make."""
    with (
        patch("esphome.codegen.get_variable", new_callable=AsyncMock) as get_variable,
        patch("esphome.codegen.new_Pvariable") as new_pvariable,
        patch(
            "esphome.codegen.register_parented", new_callable=AsyncMock
        ) as register_parented,
        patch("esphome.cpp_generator.add_global") as add_global,
        patch(
            "esphome.cpp_generator.add_global_after_includes"
        ) as add_global_after_includes,
        patch("esphome.codegen.add") as add,
    ):
        get_variable.return_value = PARENT_OBJ
        new_pvariable.return_value = NEW_OBJ
        calls = MagicMock()
        calls.attach_mock(new_pvariable, "new_pvariable")
        calls.attach_mock(add_global, "add_global")
        calls.attach_mock(add_global_after_includes, "add_global_after_includes")
        yield MockCodegen(
            get_variable,
            new_pvariable,
            register_parented,
            add_global,
            add_global_after_includes,
            add,
            calls,
        )


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


async def _run_entry(
    entry: RegistryEntry,
    config: dict[str, object],
    args: list[tuple[object, str]] | None,
    platform: str,
    id_key: str = CONF_ID,
) -> RegistryEntry:
    """Run a registered builder with the given config, trigger args and platform."""
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
    args = args or []
    template_arg = cg.TemplateArguments(*(t for t, _ in args))
    await entry.fun({id_key: PARENT_ID, **config}, ID("obj_1"), template_arg, args)
    return entry


async def _run_apply_action(
    registries: tuple[Registry, Registry],
    fields: tuple[ApplyField | ApplyCall, ...],
    config: dict[str, object],
    args: list[tuple[object, str]] | None = None,
    call: str | None = None,
    platform: str = "esp32",
    id_key: str = CONF_ID,
) -> RegistryEntry:
    """Register an apply action and run its builder with the given config."""
    actions, _ = registries
    register_apply_action("my.apply", None, *fields, call=call, id_key=id_key)
    return await _run_entry(actions["my.apply"], config, args, platform, id_key)


async def _run_apply_condition(
    registries: tuple[Registry, Registry],
    check: str | ApplyCall,
    config: dict[str, object],
    args: list[tuple[object, str]] | None = None,
    platform: str = "esp32",
    id_key: str = CONF_ID,
    defaults: dict[str, object] | None = None,
) -> RegistryEntry:
    """Register an apply condition and run its builder with the given config."""
    _, conditions = registries
    register_apply_condition("my.check", None, check, id_key=id_key, defaults=defaults)
    return await _run_entry(conditions["my.check"], config, args, platform, id_key)


def _apply_definition(mock_cg: MockCodegen) -> str:
    return str(mock_cg.add_global_after_includes.call_args.args[0])


@pytest.mark.asyncio
async def test_register_apply_action_entry(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    entry = await _run_apply_action(registries, (), {}, args=[(cg.int32, "x")])
    assert entry.type_id is ApplyAction
    assert entry.synchronous is True
    mock_cg.get_variable.assert_awaited_once_with(PARENT_ID)
    action_id, template_arg = mock_cg.new_pvariable.call_args.args
    assert action_id == ID("obj_1")
    assert str(template_arg) == "<esphome__obj_1__fn, int32_t>"
    # The declaration must precede the storage line that names the function; the body
    # goes after the user includes.
    assert [c[0] for c in mock_cg.calls.mock_calls] == [
        "add_global",
        "add_global_after_includes",
        "new_pvariable",
    ]
    assert str(mock_cg.add_global.call_args.args[0]) == (
        "static void esphome__obj_1__fn(const std::remove_cvref_t<int32_t> & x);"
    )
    assert _apply_definition(mock_cg).startswith(
        "static void esphome__obj_1__fn(const std::remove_cvref_t<int32_t> & x) {"
    )


@pytest.mark.asyncio
async def test_apply_custom_id_key(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    await _run_apply_action(registries, (), {}, id_key="transmitter_id")
    mock_cg.get_variable.assert_awaited_once_with(PARENT_ID)
    mock_cg.get_variable.reset_mock()
    await _run_apply_condition(registries, "is_on()", {}, id_key="transmitter_id")
    mock_cg.get_variable.assert_awaited_once_with(PARENT_ID)


@pytest.mark.asyncio
async def test_apply_constants(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """Constants are immediates, strings stay in flash, absent keys emit nothing, order is kept."""
    fields = (
        ApplyField("kp", "set_kp", cg.float_),
        ApplyField("ki", "set_ki", cg.float_),
        ApplyField("on", "set_on", cg.bool_),
        ApplyField("song", "play", cg.std_string),
        ApplyField("position", "position = {}", cg.float_),
        ApplyCall("publish_state()"),
    )
    config = {"kp": 0.0, "on": False, "song": "a:b", "position": 0.5}
    await _run_apply_action(registries, fields, config)
    text = _apply_definition(mock_cg)
    lines = [
        f"::{PARENT_OBJ}->set_kp(0.0f);",
        f"::{PARENT_OBJ}->set_on(false);",
        f'::{PARENT_OBJ}->play("a:b");',
        f"::{PARENT_OBJ}->position = 0.5f;",
        f"::{PARENT_OBJ}->publish_state();",
    ]
    positions = [text.index(line) for line in lines]
    assert positions == sorted(positions)
    assert "set_ki" not in text


@pytest.mark.asyncio
async def test_apply_id_constant_is_the_named_object(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """A templatable use_id given as a plain id renders the object it names."""
    target = MockObj("speaker_b", "->")
    mock_cg.get_variable.side_effect = [PARENT_OBJ, target]
    fields = (ApplyField("target", "switch_to_output", cg.RawExpression("Speaker *")),)
    await _run_apply_action(registries, fields, {"target": ID("speaker_b")})
    mock_cg.get_variable.assert_any_await(ID("speaker_b"))
    assert f"::{PARENT_OBJ}->switch_to_output(::speaker_b);" in str(
        _apply_definition(mock_cg)
    )


@pytest.mark.asyncio
async def test_apply_condition_id_constant_is_the_named_object(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """The condition path resolves and qualifies an id constant the same way."""
    target = MockObj("speaker_b", "->")
    mock_cg.get_variable.side_effect = [PARENT_OBJ, target]
    check = ApplyCall("is_output({})", (("target", cg.RawExpression("Speaker *")),))
    await _run_apply_condition(registries, check, {"target": ID("speaker_b")})
    mock_cg.get_variable.assert_any_await(ID("speaker_b"))
    assert f"return ::{PARENT_OBJ}->is_output(::speaker_b);" in str(
        _apply_definition(mock_cg)
    )


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
    text = _apply_definition(mock_cg)
    assert text.startswith(
        "static void esphome__obj_1__fn(const std::remove_cvref_t<int32_t> & x) {"
    )
    # The parent is global-scope qualified, so an arg named like the id cannot shadow it.
    assert f"::{PARENT_OBJ}->set_kp(" in text
    assert f"::{PARENT_OBJ}->set_kp(static_cast<float>(x * 2));" in text
    # Outer apply lambda and inner field lambda spell the trigger arg identically.
    assert text.count("const std::remove_cvref_t<int32_t> & x") == 2
    assert (
        f"::{PARENT_OBJ}->set_ki([](const std::remove_cvref_t<int32_t> & x) -> float {{"
        in text
    )
    assert "}(x));" in text


@pytest.mark.asyncio
async def test_apply_lambdas_by_value(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """With by_value a lambda takes the trigger args by value and is called, never reduced."""
    fields = (
        ApplyField("kp", "set_kp", cg.float_, by_value=True),
        ApplyCall(
            "set_range({}, {})",
            (("low", cg.float_), ("high", cg.float_)),
            by_value=True,
        ),
    )
    config = {
        "kp": Lambda("return x *= 2;"),
        "low": Lambda("x += 1; return x;"),
        "high": 1.0,
    }
    await _run_apply_action(registries, fields, config, args=[(cg.int32, "x")])
    text = _apply_definition(mock_cg)
    assert text.startswith(
        "static void esphome__obj_1__fn(const std::remove_cvref_t<int32_t> & x) {"
    )
    assert f"::{PARENT_OBJ}->set_kp([](int32_t x) -> float {{" in text
    assert f"::{PARENT_OBJ}->set_range([](int32_t x) -> float {{" in text
    assert text.count("}(x)") == 2
    assert "static_cast" not in text


@pytest.mark.asyncio
async def test_apply_condition_lambda_by_value(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """A by_value condition lambda takes the trigger args by value and is called."""
    check = ApplyCall("state == {}", (("state", cg.float_),), by_value=True)
    await _run_apply_condition(
        registries, check, {"state": Lambda("return x *= 2;")}, args=[(cg.int32, "x")]
    )
    text = _apply_definition(mock_cg)
    assert f"return ::{PARENT_OBJ}->state == [](int32_t x) -> float {{" in text
    assert "}(x);" in text


@pytest.mark.asyncio
async def test_apply_call_keys(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """A multi-key call needs all keys, is skipped with none, and errors on a partial set."""
    fields = (
        ApplyCall("set_range({}, {})", (("low", cg.float_), ("high", cg.float_))),
    )
    await _run_apply_action(registries, fields, {"low": 1.0, "high": 2.0})
    assert f"::{PARENT_OBJ}->set_range(1.0f, 2.0f);" in _apply_definition(mock_cg)

    mock_cg.new_pvariable.reset_mock()
    await _run_apply_action(registries, fields, {})
    assert "set_range" not in _apply_definition(mock_cg)

    with pytest.raises(EsphomeError, match="needs all of"):
        await _run_apply_action(registries, fields, {"low": 1.0})


@pytest.mark.asyncio
async def test_apply_action_call_shape(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    fields = (ApplyField("brightness", "set_brightness", cg.float_),)
    await _run_apply_action(registries, fields, {"brightness": 0.5}, call="make_call")
    text = _apply_definition(mock_cg)
    lines = [
        f"auto apply_call = ::{PARENT_OBJ}->make_call();",
        "apply_call.set_brightness(0.5f);",
        "apply_call.perform();",
    ]
    positions = [text.index(line) for line in lines]
    assert positions == sorted(positions)


@pytest.mark.asyncio
async def test_apply_target_names_parent(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """{parent} in a target is the global-scoped parent, also beside a call object."""
    fields = (ApplyCall("set_peer({parent}->id(), {})", (("level", cg.float_),)),)
    await _run_apply_action(registries, fields, {"level": 0.5})
    assert (
        f"::{PARENT_OBJ}->set_peer(::{PARENT_OBJ}->id(), 0.5f);"
        in _apply_definition(mock_cg)
    )
    mock_cg.new_pvariable.reset_mock()
    await _run_apply_action(registries, fields, {"level": 0.5}, call="make_call")
    assert f"apply_call.set_peer(::{PARENT_OBJ}->id(), 0.5f);" in _apply_definition(
        mock_cg
    )
    mock_cg.new_pvariable.reset_mock()
    await _run_apply_condition(registries, "is_peer({parent}->id())", {})
    assert (
        f"return ::{PARENT_OBJ}->is_peer(::{PARENT_OBJ}->id());"
        in _apply_definition(mock_cg)
    )


@pytest.mark.asyncio
async def test_apply_field_nested_key_const_fn_and_type_string(
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
        ApplyField("value", "value() = {}", "decltype({parent}->value())"),
    )
    config = {
        "vertical": {"direction": 3},
        "name": "abc",
        "value": Lambda("return 42;"),
    }
    await _run_apply_action(registries, fields, config)
    text = _apply_definition(mock_cg)
    assert f"::{PARENT_OBJ}->set_direction(3);" in text
    assert f'::{PARENT_OBJ}->set_name("abc", 3);' in text
    assert (
        f"::{PARENT_OBJ}->value() = static_cast<decltype(::{PARENT_OBJ}->value())>(42);"
        in text
    )

    mock_cg.new_pvariable.reset_mock()
    await _run_apply_action(registries, fields[:1], {})
    assert "set_direction" not in _apply_definition(mock_cg)


def test_apply_registration_checks(registries: tuple[Registry, Registry]) -> None:
    with pytest.raises(ValueError, match="2 placeholder"):
        ApplyCall("set_range({}, {})", (("low", cg.float_),))
    with pytest.raises(ValueError, match="only {} and {parent}"):
        ApplyCall("if ({}) {other}->reset()", (("reset", cg.bool_),))
    assert ApplyCall("if ({}) {parent}->reset()", (("reset", cg.bool_),)).names_parent
    assert not ApplyCall("set_flags({{parent}})").names_parent
    ApplyCall("set_flags({{{}}})", (("flags", cg.int_),))
    with pytest.raises(ValueError, match="each arg is"):
        ApplyCall("set_kp({})", (("kp", cg.float_, None, "extra"),))
    schema = cv.Schema({cv.Required(CONF_ID): cv.string, cv.Optional("kp"): cv.float_})
    register_apply_action("my.ok", schema, ApplyField("kp", "set_kp", cg.float_))
    with pytest.raises(ValueError, match="'kd' is not in the schema"):
        register_apply_action("my.bad", schema, ApplyField("kd", "set_kd", cg.float_))
    register_apply_condition(
        "my.is", schema, ApplyCall("kp == {}", (("kp", cg.float_),))
    )
    with pytest.raises(ValueError, match="'kd' is not in the schema"):
        register_apply_condition(
            "my.bad_is", schema, ApplyCall("kd == {}", (("kd", cg.float_),))
        )
    with pytest.raises(ValueError, match="'parent_id' is not in the schema"):
        register_apply_action("my.bad_id", schema, id_key="parent_id")
    with pytest.raises(ValueError, match="'parent_id' is not in the schema"):
        register_apply_condition("my.bad_is_id", schema, "is_on()", id_key="parent_id")
    either = cv.Any(schema, cv.Schema({cv.Optional("kd"): cv.float_}))
    register_apply_action("my.any", either, ApplyField("kd", "set_kd", cg.float_))
    for wrapped in (
        maybe_simple_id(schema),
        maybe_simple_id(schema.schema),
        cv.All(schema),
        cv.maybe_simple_value(schema, key="kp"),
    ):
        with pytest.raises(ValueError, match="'kd' is not in the schema"):
            register_apply_action(
                "my.bad", wrapped, ApplyField("kd", "set_kd", cg.float_)
            )
    nested = cv.Schema(
        {
            cv.Required(CONF_ID): cv.string,
            cv.Optional("v"): cv.Schema({cv.Optional("dir"): cv.int_}),
        }
    )
    register_apply_action(
        "my.nested", nested, ApplyField(("v", "dir"), "set_dir", cg.int_)
    )
    with pytest.raises(ValueError, match="'dri' is not in the schema"):
        register_apply_action(
            "my.bad2", nested, ApplyField(("v", "dri"), "set_dir", cg.int_)
        )


@pytest.mark.asyncio
async def test_apply_string_constant_stays_in_flash_on_esp8266(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    fields = (ApplyField("song", "play", cg.std_string),)
    await _run_apply_action(registries, fields, {"song": "a:b"}, platform="esp8266")
    assert (
        f'::{PARENT_OBJ}->play(progmem_string(ESPHOME_F("a:b")));'
        in _apply_definition(mock_cg)
    )


@pytest.mark.asyncio
@pytest.mark.parametrize("platform", ["esp32", "esp8266"])
async def test_apply_literal_with_length_is_plain_on_every_platform(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen, platform: str
) -> None:
    """A (const char *, size_t) target gets the RAM literal and its byte length, never a flash copy."""
    fields = (
        ApplyField("option", "set_option", cg.std_string, const_fn=literal_with_length),
    )
    await _run_apply_action(
        registries, fields, {"option": "h\u00e9llo"}, platform=platform
    )
    text = _apply_definition(mock_cg)
    assert f'::{PARENT_OBJ}->set_option("h\\303\\251llo", 6);' in text
    assert "progmem_string" not in text


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("platform", "rendered"),
    [("esp32", 'StringRef("ON", 2)'), ("esp8266", 'ESPHOME_F("ON")')],
)
async def test_apply_string_ref_literal_stays_in_flash_on_esp8266(
    registries: tuple[Registry, Registry],
    mock_cg: MockCodegen,
    platform: str,
    rendered: str,
) -> None:
    fields = (
        ApplyField(
            "payload", "set_payload", cg.std_string, const_fn=string_ref_literal
        ),
    )
    await _run_apply_action(registries, fields, {"payload": "ON"}, platform=platform)
    assert f"::{PARENT_OBJ}->set_payload({rendered});" in _apply_definition(mock_cg)


@pytest.mark.asyncio
@pytest.mark.parametrize("platform", ["esp32", "esp8266"])
@pytest.mark.parametrize(
    "body", ["x *= 2;\nreturn {0x01, x};", "return std::vector<uint8_t>{++x};"]
)
async def test_apply_bytes_lambda_is_a_vector_in_a_block(
    registries: tuple[Registry, Registry],
    mock_cg: MockCodegen,
    platform: str,
    body: str,
) -> None:
    """A lambda payload is called once into a scoped vector and passed as data and size;
    it takes the trigger args by their own types and is never reduced, so it may modify them."""
    fields = (ApplyBytes("data", "send({})"),)
    config = {"data": Lambda(body)}
    await _run_apply_action(
        registries, fields, config, args=[(cg.uint8, "x")], platform=platform
    )
    text = _apply_definition(mock_cg)
    assert text.startswith(
        "static void esphome__obj_1__fn(const std::remove_cvref_t<uint8_t> & x) {"
    )
    assert (
        "const std::vector<uint8_t> esphome__bytes = [](uint8_t x) -> std::vector<uint8_t> {"
        in text
    )
    assert "}(x);" in text
    assert (
        f"::{PARENT_OBJ}->send(esphome__bytes.data(), esphome__bytes.size());" in text
    )
    assert "progmem_memcpy" not in text


@pytest.mark.asyncio
async def test_apply_bytes_keeps_a_mutable_reference_arg(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """A reference trigger arg such as climate's `ClimateCall &` is passed through as is."""
    fields = (ApplyBytes("data", "send({})"),)
    config = {"data": Lambda("return {0x01};")}
    call_ref = cg.MockObj("ClimateCall").operator("ref")
    await _run_apply_action(registries, fields, config, args=[(call_ref, "x")])
    text = _apply_definition(mock_cg)
    assert text.startswith("static void esphome__obj_1__fn(ClimateCall & x) {")
    assert "[](ClimateCall & x) -> std::vector<uint8_t> {" in text
    assert "}(x);" in text


@pytest.mark.asyncio
async def test_apply_bytes_constant_is_a_shared_table(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """A constant payload is the shared PROGMEM table and its length, named after the action."""
    CORE.config = {}
    fields = (ApplyBytes("data", "send({})"),)
    await _run_apply_action(registries, fields, {"data": [0xA1, 0x02]})
    assert f"::{PARENT_OBJ}->send(::esphome__my_apply__bytes, 2);" in _apply_definition(
        mock_cg
    )

    mock_cg.new_pvariable.reset_mock()
    await _run_apply_action(registries, fields, {"data": b""})
    assert f"::{PARENT_OBJ}->send(nullptr, 0);" in _apply_definition(mock_cg)

    mock_cg.new_pvariable.reset_mock()
    await _run_apply_action(registries, fields, {})
    assert "send(" not in _apply_definition(mock_cg)

    with pytest.raises(EsphomeError, match="maximum is 65535"):
        await _run_apply_action(registries, fields, {"data": bytes(0x10000)})


@pytest.mark.asyncio
async def test_apply_bytes_constant_is_copied_out_of_flash_on_esp8266(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    """On ESP8266 a constant goes through a stack buffer, or straight to a flash-aware target."""
    CORE.config = {}
    fields = (ApplyBytes("data", "send({})", stack=128),)
    await _run_apply_action(
        registries, fields, {"data": [0xA1, 0x02]}, platform="esp8266"
    )
    text = _apply_definition(mock_cg)
    lines = [
        "uint8_t esphome__bytes[2];",
        "esphome::progmem_memcpy(esphome__bytes, ::esphome__my_apply__bytes, 2);",
        f"::{PARENT_OBJ}->send(esphome__bytes, 2);",
    ]
    positions = [text.index(line) for line in lines]
    assert positions == sorted(positions)

    # Past the stack budget the copy goes to the heap
    mock_cg.new_pvariable.reset_mock()
    fields = (ApplyBytes("data", "send({})", stack=1),)
    await _run_apply_action(
        registries, fields, {"data": [0xA1, 0x02]}, platform="esp8266"
    )
    text = _apply_definition(mock_cg)
    assert "auto esphome__bytes = std::make_unique<uint8_t[]>(2);" in text
    assert (
        "esphome::progmem_memcpy(esphome__bytes.get(), ::esphome__my_apply__bytes, 2);"
        in text
    )
    assert f"::{PARENT_OBJ}->send(esphome__bytes.get(), 2);" in text

    mock_cg.new_pvariable.reset_mock()
    fields = (ApplyBytes("data", "send({})", progmem_target="send_progmem({})"),)
    await _run_apply_action(
        registries, fields, {"data": [0xA1, 0x02]}, platform="esp8266"
    )
    text = _apply_definition(mock_cg)
    assert f"::{PARENT_OBJ}->send_progmem(::esphome__my_apply__bytes, 2);" in text
    assert "progmem_memcpy" not in text

    # An empty payload never touches flash but still passes a non-null buffer
    mock_cg.new_pvariable.reset_mock()
    fields = (ApplyBytes("data", "send({})"),)
    await _run_apply_action(registries, fields, {"data": []}, platform="esp8266")
    text = _apply_definition(mock_cg)
    assert "uint8_t esphome__bytes[1];" in text
    assert f"::{PARENT_OBJ}->send(esphome__bytes, 0);" in text
    assert "progmem_memcpy" not in text


@pytest.mark.asyncio
async def test_apply_bytes_beside_other_fields_and_call_object(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    CORE.config = {}
    fields = (
        ApplyField("channel", "set_channel", cg.uint8),
        ApplyBytes("data", "write({})"),
    )
    await _run_apply_action(
        registries, fields, {"channel": 3, "data": [1]}, call="make_call"
    )
    text = _apply_definition(mock_cg)
    lines = [
        f"auto apply_call = ::{PARENT_OBJ}->make_call();",
        "apply_call.set_channel(3);",
        "apply_call.write(::esphome__my_apply__bytes, 1);",
        "apply_call.perform();",
    ]
    positions = [text.index(line) for line in lines]
    assert positions == sorted(positions)


def test_apply_bytes_checks_its_shape(registries: tuple[Registry, Registry]) -> None:
    # Escaped braces are not placeholders
    assert ApplyBytes("data", "send({{}}, {})").target == "send({{}}, {})"
    with pytest.raises(ValueError, match="needs one {}"):
        ApplyBytes("data", "send({{}})")
    with pytest.raises(ValueError, match="needs one {}"):
        ApplyBytes("data", "send()")
    with pytest.raises(ValueError, match="needs one {}"):
        ApplyBytes("data", "send({})", progmem_target="send_progmem({}, {})")
    with pytest.raises(ValueError, match="'dat' is not in the schema"):
        register_apply_action(
            "my.bad",
            cv.Schema(
                {cv.Required(CONF_ID): cv.string, cv.Required("data"): cv.string}
            ),
            ApplyBytes("dat", "send({})"),
        )


@pytest.mark.asyncio
async def test_register_apply_condition_predicate(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    entry = await _run_apply_condition(
        registries, "is_playing()", {}, args=[(cg.int32, "x")]
    )
    assert entry.type_id is ApplyCondition
    condition_id, template_arg = mock_cg.new_pvariable.call_args.args
    assert condition_id == ID("obj_1")
    assert str(template_arg) == "<esphome__obj_1__fn, int32_t>"
    text = _apply_definition(mock_cg)
    assert text.startswith(
        "static bool esphome__obj_1__fn(const std::remove_cvref_t<int32_t> & x) {"
    )
    assert f"return ::{PARENT_OBJ}->is_playing();" in text


@pytest.mark.asyncio
async def test_apply_condition_compares_config_value(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    check = ApplyCall("state == {}", (("state", cg.bool_),))
    await _run_apply_condition(registries, check, {"state": True})
    assert f"return ::{PARENT_OBJ}->state == true;" in _apply_definition(mock_cg)

    with pytest.raises(EsphomeError, match="needs all of"):
        await _run_apply_condition(registries, check, {})


@pytest.mark.asyncio
async def test_apply_condition_defaults_fill_absent_keys(
    registries: tuple[Registry, Registry], mock_cg: MockCodegen
) -> None:
    config = {"above": 10.0}
    await _run_apply_condition(
        registries, IN_RANGE_CHECK, config, defaults=IN_RANGE_DEFAULTS
    )
    assert (
        f"return ::{PARENT_OBJ}->state >= 10.0f && ::{PARENT_OBJ}->state <= INFINITY;"
        in _apply_definition(mock_cg)
    )
    assert config == {"above": 10.0}


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("platform", "value", "expected"),
    [
        ("esp32", "two", '"two"'),
        ("esp8266", "two", '::esphome::ProgmemStringRef{PSTR("two"), 3}'),
        # The length is in bytes, not characters
        ("esp8266", "é", '::esphome::ProgmemStringRef{PSTR("\\303\\251"), 2}'),
    ],
)
async def test_apply_condition_string_constant(
    registries: tuple[Registry, Registry],
    mock_cg: MockCodegen,
    platform: str,
    value: str,
    expected: str,
) -> None:
    check = ApplyCall("state == {}", (("state", cg.std_string),))
    await _run_apply_condition(registries, check, {"state": value}, platform=platform)
    assert f"return ::{PARENT_OBJ}->state == {expected};" in _apply_definition(mock_cg)


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("body", "expected", "called"),
    [
        ("return x;", "->state == (x);", False),
        ('return x.empty() ? "e" : x;', '->state == (x.empty() ? "e" : x);', False),
        ('if (x.empty()) return "e";\nreturn x;', "}(x);", True),
    ],
)
async def test_apply_condition_string_lambda_paths(
    registries: tuple[Registry, Registry],
    mock_cg: MockCodegen,
    body: str,
    expected: str,
    called: bool,
) -> None:
    """A single return is inlined with no copy; a longer body is a called std::string lambda."""
    check = ApplyCall("state == {}", (("state", cg.std_string),))
    await _run_apply_condition(
        registries, check, {"state": Lambda(body)}, args=[(cg.std_string, "x")]
    )
    text = _apply_definition(mock_cg)
    assert expected in text
    assert ("-> std::string {" in text) is called


def test_progmem_bytes_shares_equal_payloads() -> None:
    CORE.config = {}
    a = progmem_bytes("payload", [1, 2])
    b = progmem_bytes("payload", b"\x01\x02")
    assert a is b
    assert str(progmem_bytes("payload", [])) == "nullptr"


@pytest.mark.asyncio
async def test_templatable_bytes_static_payload() -> None:
    CORE.config = {}
    var = MockObj("act", "->")
    await templatable_bytes(
        [0xA1, 0x02], [], var.set_code_template, var.set_code_static, "payload"
    )
    assert "act->set_code_static(payload, 2);" in CORE.cpp_main_section


@pytest.mark.asyncio
async def test_templatable_bytes_lambda_payload() -> None:
    CORE.config = {}
    var = MockObj("act", "->")
    await templatable_bytes(
        Lambda("return {0x01, 0x02};"),
        [],
        var.set_code_template,
        var.set_code_static,
        "payload",
    )
    text = CORE.cpp_main_section
    assert "act->set_code_template(" in text
    assert "-> std::vector<uint8_t>" in text
    assert "set_code_static" not in text


@pytest.mark.asyncio
async def test_templatable_bytes_rejects_oversized_payload() -> None:
    CORE.config = {}
    var = MockObj("act", "->")
    with pytest.raises(EsphomeError, match="maximum is 65535"):
        await templatable_bytes(
            [0] * 0x10000, [], var.set_code_template, var.set_code_static, "payload"
        )


TRIGGER_CONF = {CONF_AUTOMATION_ID: ID("automation_1"), CONF_THEN: []}


def _squash(expr: object) -> str:
    """Generated text with its whitespace folded, for one-line assertions."""
    return " ".join(str(expr).split())


def test_parent_ref_is_global_scoped() -> None:
    """A parent named through parent_ref cannot be shadowed by a trigger argument."""
    assert (
        str(parent_ref(MockObj("sel", "->")).option_at(RawExpression("i")))
        == "::sel->option_at(i)"
    )


@pytest.mark.asyncio
async def test_trigger_callback_forwards_params(mock_cg: MockCodegen) -> None:
    """Without forward or when the callback passes its parameters straight to trigger()."""
    text = str(
        await build_trigger_callback(
            [(cg.bool_, "state")], TRIGGER_CONF, [(cg.bool_, "state")]
        )
    )
    assert text.startswith("[](const std::remove_cvref_t<bool> & state) -> void {")
    assert f"::{NEW_OBJ}->trigger(state);" in text
    assert "if (" not in text


@pytest.mark.asyncio
async def test_trigger_callback_reshapes_and_filters(mock_cg: MockCodegen) -> None:
    """A filter returns early, forward picks the trigger args, a string constant is a plain literal."""
    when = ApplyCall("payload == {}", (("payload", cg.std_string),))
    params = [(cg.std_string, "topic"), (cg.std_string, "payload")]
    text = _squash(
        await build_trigger_callback(
            [(cg.std_string, "x")],
            {**TRIGGER_CONF, "payload": "hi"},
            params,
            forward=["payload"],
            when=when,
        )
    )
    assert "& topic, const std::remove_cvref_t<std::string> & payload) -> void" in text
    assert f'if (!(payload == "hi")) return; ::{NEW_OBJ}->trigger(payload);' in text
    # An absent optional key skips the filter, as for any ApplyCall.
    text = str(
        await build_trigger_callback(
            [(cg.std_string, "x")], TRIGGER_CONF, params, forward=["payload"], when=when
        )
    )
    assert "if (" not in text


@pytest.mark.asyncio
async def test_trigger_callback_filter_lambda_by_value(mock_cg: MockCodegen) -> None:
    """A by_value filter lambda takes the callback parameters by value and is called."""
    when = ApplyCall("state == {}", (("state", cg.int_),), by_value=True)
    text = _squash(
        await build_trigger_callback(
            [],
            {**TRIGGER_CONF, "state": Lambda("return x *= 2;")},
            [(cg.int_, "x")],
            forward=[],
            when=when,
        )
    )
    assert "if (!(state == [](int x) -> int {" in text
    assert "}(x))) return;" in text


@pytest.mark.asyncio
async def test_trigger_callback_filter_has_no_parent(mock_cg: MockCodegen) -> None:
    """A filter that names {parent} is rejected up front when no parent is given."""
    for when in (
        ApplyCall("mode == {}", (("mode", "{parent}::Mode"),)),
        "{parent}->is_failed() == false",
    ):
        with pytest.raises(ValueError, match="names {parent}"):
            await build_trigger_callback(
                [],
                {**TRIGGER_CONF, "mode": 1},
                [(cg.int_, "mode")],
                forward=[],
                when=when,
            )


@pytest.mark.asyncio
async def test_trigger_callback_filter_names_parent(mock_cg: MockCodegen) -> None:
    """With the parent given, {parent} in a filter is the global-scoped parent."""
    text = _squash(
        await build_trigger_callback(
            [],
            TRIGGER_CONF,
            [(cg.int_, "state")],
            forward=[],
            when="state == 1 && {parent}->is_failed() == false",
            parent=MockObj("improv", "->"),
        )
    )
    assert (
        "if (!(state == 1 && ::improv->is_failed() == false)) return; "
        f"::{NEW_OBJ}->trigger();"
    ) in text


@pytest.mark.asyncio
async def test_build_callback_automations_when_names_parent(
    mock_cg: MockCodegen,
) -> None:
    """A table entry names the parent through {parent}, so it can live at module level."""
    entries = (
        CallbackAutomation(
            "on_open", "add_on_state_callback", when="{parent}->is_fully_open()"
        ),
    )
    await build_callback_automations(
        MockObj("valve", "->"), {"on_open": [TRIGGER_CONF]}, entries
    )
    assert _squash(mock_cg.add.call_args.args[0]) == (
        "valve->add_on_state_callback([]() -> void { "
        f"if (!(::valve->is_fully_open())) return; ::{NEW_OBJ}->trigger(); }})"
    )


@pytest.mark.asyncio
async def test_build_parent_callback_automation(mock_cg: MockCodegen) -> None:
    """A no-argument callback registers a lambda that hands the parent to the automation."""
    parent = MockObj("fan", "->")
    await build_parent_callback_automation(
        parent,
        "add_on_state_callback",
        (cg.RawExpression("Fan *"), "x"),
        TRIGGER_CONF,
    )
    assert _squash(mock_cg.add.call_args.args[0]) == (
        f"fan->add_on_state_callback([]() -> void {{ ::{NEW_OBJ}->trigger(::fan); }})"
    )


@pytest.mark.asyncio
async def test_build_callback_automation_lambda(mock_cg: MockCodegen) -> None:
    """Reshaping keywords switch the registration to the lambda."""
    parent = MockObj("sel", "->")
    await build_callback_automation(
        parent,
        "add_cb",
        [(cg.std_string, "x"), (cg.size_t, "i")],
        TRIGGER_CONF,
        params=[(cg.size_t, "index")],
        forward=[parent_ref(parent).option_at(RawExpression("index")), "index"],
    )
    assert _squash(mock_cg.add.call_args.args[0]) == (
        "sel->add_cb([](const std::remove_cvref_t<size_t> & index) -> void { "
        f"::{NEW_OBJ}->trigger(::sel->option_at(index), index); }})"
    )


@pytest.mark.asyncio
async def test_build_callback_automation_forwarder_with_filter(
    mock_cg: MockCodegen,
) -> None:
    """When objects are needed, a forwarder given with a filter is the one registered."""
    await build_callback_automation(
        MockObj("parent", "->"),
        "add_cb",
        [],
        TRIGGER_CONF,
        forwarder=TriggerOnTrueForwarder,
        params=[(cg.bool_, "state")],
        forward=[],
        when="state",
    )
    assert str(mock_cg.add.call_args.args[0]) == (
        f"parent->add_cb(TriggerOnTrueForwarder{{{NEW_OBJ}}})"
    )


@pytest.mark.asyncio
async def test_build_callback_automation_forwarder(mock_cg: MockCodegen) -> None:
    """The forwarder path registers a pointer-sized TriggerForwarder, not a lambda."""
    await build_callback_automation(
        MockObj("parent", "->"), "add_cb", [(cg.bool_, "x")], TRIGGER_CONF
    )
    assert str(mock_cg.add.call_args.args[0]) == (
        f"parent->add_cb(TriggerForwarder<bool>{{{NEW_OBJ}}})"
    )


def test_if_accepts_bare_condition() -> None:
    """A single condition directly under `if:` is the same as `condition: {...}`."""
    bare = validate_action({"if": {"or": [], CONF_THEN: [{"delay": "1s"}]}})
    wrapped = validate_action(
        {"if": {CONF_CONDITION: {"or": []}, CONF_THEN: [{"delay": "1s"}]}}
    )
    assert bare["if"][CONF_CONDITION] == wrapped["if"][CONF_CONDITION]
    assert "or" not in bare["if"]


@pytest.mark.parametrize("key", [CONF_ANY, CONF_ALL])
def test_if_any_all_are_not_bare_conditions(key: str) -> None:
    """`any:` and `all:` under `if:` are the if action's own keys, not a bare condition."""
    result = validate_action({"if": {key: [{"or": []}], CONF_THEN: [{"delay": "1s"}]}})
    assert key in result["if"]
    assert CONF_CONDITION not in result["if"]


@pytest.mark.parametrize(
    ("value", "match"),
    [
        pytest.param(
            {"or": [], "and": [], CONF_THEN: []},
            "Only one condition",
            id="two_bare",
        ),
        pytest.param(
            {"or": [], CONF_CONDITION: {"and": []}, CONF_THEN: []},
            "cannot be used with 'condition'",
            id="bare_and_wrapped",
        ),
        pytest.param(
            {"or": [], CONF_ANY: [{"and": []}], CONF_THEN: []},
            "cannot be used with 'any'",
            id="bare_and_any",
        ),
    ],
)
def test_if_rejects_ambiguous_bare_condition(
    value: dict[str, object], match: str
) -> None:
    with pytest.raises(cv.Invalid, match=match):
        validate_action({"if": value})


def test_if_with_non_dict_value_is_invalid() -> None:
    with pytest.raises(cv.Invalid, match="expected a dictionary"):
        validate_action({"if": "not a dict"})


CHOICE = {"if": {"and": []}, CONF_THEN: [{"delay": "1s"}]}


@pytest.mark.parametrize(
    ("value", "match"),
    [
        pytest.param([], "at least 1", id="empty"),
        pytest.param([{CONF_THEN: [{"delay": "1s"}]}], "'if' is required", id="no_if"),
        pytest.param([{"if": {"and": []}}], "'then' is required", id="no_then"),
        pytest.param(
            [{CONF_ELSE: [{"delay": "1s"}]}],
            "At least one choice must have 'if' and 'then'",
            id="only_else",
        ),
        pytest.param(
            [{CONF_ELSE: [{"delay": "1s"}]}, CHOICE],
            "Only the last choice may be 'else'",
            id="else_not_last",
        ),
        pytest.param(
            [CHOICE, {"if": {"and": []}, CONF_ELSE: []}],
            "'if' cannot be used with 'else'",
            id="else_with_if",
        ),
        pytest.param(
            [CHOICE, {CONF_THEN: [], CONF_ELSE: []}],
            "'then' cannot be used with 'else'",
            id="else_with_then",
        ),
    ],
)
def test_choose_invalid(value: list[ConfigType], match: str) -> None:
    with pytest.raises(cv.Invalid, match=match):
        validate_action({CONF_CHOOSE: value})


def test_choose_valid() -> None:
    result = validate_action(
        {
            CONF_CHOOSE: [
                CHOICE,
                {"if": [{"or": []}, {"and": []}], CONF_THEN: [{"delay": "2s"}]},
                {CONF_ELSE: [{"delay": "3s"}]},
            ]
        }
    )

    choices = result[CONF_CHOOSE]
    assert len(choices) == 3
    assert "and" in choices[1]["if"]
    assert CONF_ELSE in choices[2]
    assert all(CONF_CHOICE_ID in choice for choice in choices)


@pytest.mark.asyncio
@pytest.mark.parametrize("has_else", [True, False])
async def test_choose_builds_if_else_chain(has_else: bool) -> None:
    """Each choice is an IfAction whose else branch holds the next choice, then the else actions."""
    config: list[ConfigType] = [
        {"if": "cond_a", CONF_THEN: "then_a", CONF_CHOICE_ID: ID("choice_a")},
        {"if": "cond_b", CONF_THEN: "then_b", CONF_CHOICE_ID: ID("choice_b")},
    ]
    if has_else:
        config.append({CONF_ELSE: "else_c", CONF_CHOICE_ID: ID("choice_c")})
    action_id = ID("action")
    with (
        patch(
            "esphome.automation.build_condition",
            new_callable=AsyncMock,
            side_effect=lambda conf, *_: f"built_{conf}",
        ),
        patch(
            "esphome.automation.build_action_list",
            new_callable=AsyncMock,
            side_effect=lambda conf, *_: [MockObj(f"built_{conf}")],
        ),
        patch("esphome.codegen.new_Pvariable") as new_pvariable,
        patch("esphome.codegen.add") as add,
    ):
        new_pvariable.side_effect = lambda id_, *args: MockObj(id_.id, "->")
        result = await choose_action_to_code(config, action_id, TEMPLATE_ARG, [])

    # Built from the last choice back; the first choice takes the action's own id
    created = [
        (c.args[0].id, str(c.args[1]), c.args[2]) for c in new_pvariable.call_args_list
    ]
    assert created == [
        ("choice_b", f"<{str(has_else).lower()}>", "built_cond_b"),
        ("action", "<true>", "built_cond_a"),
    ]
    assert str(result) == "action"
    statements = [str(c.args[0]) for c in add.call_args_list]
    expected_b_else = (
        ["choice_b->add_else_action(built_else_c)", "choice_b->finish_else()"]
        if has_else
        else []
    )
    assert statements == [
        "choice_b->add_then_action(built_then_b)",
        "choice_b->finish_then()",
        *expected_b_else,
        "action->add_then_action(built_then_a)",
        "action->finish_then()",
        "action->add_else_action(choice_b)",
        "action->finish_else()",
    ]


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("config", "template_arg", "expected"),
    [
        pytest.param(
            {CONF_CONDITION: "cond", CONF_THEN: "then", CONF_ELSE: "else"},
            "<true>",
            [
                "action->add_then_action(built_then)",
                "action->finish_then()",
                "action->add_else_action(built_else)",
                "action->finish_else()",
            ],
            id="then_and_else",
        ),
        pytest.param(
            {CONF_ANY: "cond", CONF_ELSE: "else"},
            "<true>",
            ["action->add_else_action(built_else)", "action->finish_else()"],
            id="only_else",
        ),
        pytest.param(
            {CONF_ALL: "cond", CONF_THEN: "then"},
            "<false>",
            ["action->add_then_action(built_then)", "action->finish_then()"],
            id="only_then",
        ),
    ],
)
async def test_if_adds_branches_one_action_at_a_time(
    config: ConfigType, template_arg: str, expected: list[str]
) -> None:
    with (
        patch(
            "esphome.automation.build_condition",
            new_callable=AsyncMock,
            side_effect=lambda conf, *_: f"built_{conf}",
        ),
        patch(
            "esphome.automation.build_action_list",
            new_callable=AsyncMock,
            side_effect=lambda conf, *_: [MockObj(f"built_{conf}")],
        ),
        patch("esphome.codegen.new_Pvariable") as new_pvariable,
        patch("esphome.codegen.add") as add,
    ):
        new_pvariable.side_effect = lambda id_, *args: MockObj(id_.id, "->")
        await if_action_to_code(config, ID("action"), TEMPLATE_ARG, [])

    new_pvariable.assert_called_once()
    assert str(new_pvariable.call_args.args[1]) == template_arg
    assert new_pvariable.call_args.args[2] == "built_cond"
    assert [str(c.args[0]) for c in add.call_args_list] == expected


def test_shadow_iteration_leaves_other_names() -> None:
    args = [(cg.std_string, "prefix"), (cg.uint32, "iteration_1")]
    assert _shadow_iteration(args) == args


def test_shadow_iteration_renames_inherited_counter() -> None:
    assert _shadow_iteration([(cg.uint32, "iteration"), (cg.bool_, "x")]) == [
        (cg.uint32, "iteration_1"),
        (cg.bool_, "x"),
    ]


def test_shadow_iteration_picks_first_unused_suffix() -> None:
    """A counter keeps its suffix at deeper levels; the new one takes the next free name."""
    args = [
        (cg.uint32, "iteration"),
        (cg.uint32, "iteration_1"),
        (cg.int_, "iteration_3"),
    ]
    assert _shadow_iteration(args) == [
        (cg.uint32, "iteration_2"),
        (cg.uint32, "iteration_1"),
        (cg.int_, "iteration_3"),
    ]


def _repeat_conf(id_: str, then: list[ConfigType]) -> ConfigType:
    return {
        "repeat": {CONF_COUNT: 2, CONF_THEN: then},
        CONF_TYPE_ID: ID(id_, is_declaration=True, type=RepeatAction),
    }


@pytest.mark.asyncio
async def test_nested_repeat_lambda_parameters_are_distinct() -> None:
    """Each repeat level adds its own `iteration`; inherited ones are renamed, not duplicated."""
    CORE.config = {}
    log = {
        "lambda": Lambda('ESP_LOGD("t", "%u", iteration);'),
        CONF_TYPE_ID: ID("log_action", is_declaration=True, type=LambdaAction),
    }
    conf = _repeat_conf(
        "outer", [_repeat_conf("middle", [_repeat_conf("inner", [log])])]
    )
    await build_action(conf, cg.TemplateArguments(cg.uint8), [(cg.uint8, "level")])
    text = CORE.cpp_main_section
    assert (
        "(uint32_t iteration, uint32_t iteration_2, uint32_t iteration_1, uint8_t level)"
        in text
    )
    assert "uint32_t iteration, uint32_t iteration)" not in text


def _render_in_range(config: dict) -> tuple[str, str]:
    config = {**IN_RANGE_DEFAULTS, **config}
    (above_key, _, low), (below_key, _, high) = IN_RANGE_CHECK.members
    return low(config, config[above_key]), high(config, config[below_key])


@pytest.mark.parametrize(
    ("config", "expected"),
    [
        ({"above": 10.0}, ("10.0f", "INFINITY")),
        ({"below": 30.0}, ("-INFINITY", "30.0f")),
        ({"above": 10.0, "below": 30.0}, ("10.0f", "30.0f")),
        # A NaN bound counted as unset before; with no lower bound a NaN upper bound stays false.
        ({"above": float("nan"), "below": 30.0}, ("-INFINITY", "30.0f")),
        ({"above": 10.0, "below": float("nan")}, ("10.0f", "INFINITY")),
        ({"above": float("nan"), "below": float("nan")}, ("-INFINITY", "NAN")),
    ],
)
def test_in_range_check_renders_bounds(config: dict, expected: tuple[str, str]) -> None:
    assert _render_in_range(config) == expected


def test_maybe_simple_id_schema_extract() -> None:
    validator = maybe_simple_id({cv.Required(CONF_ID): cv.string})
    _, key = validator(SCHEMA_EXTRACT)
    assert key == CONF_ID


def test_register_action_without_synchronous_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with patch.dict(ACTION_REGISTRY):
        register_action("test.unspecified_sync", LambdaAction, cv.Schema({}))(
            MagicMock()
        )
        entry = ACTION_REGISTRY["test.unspecified_sync"]
    assert entry.synchronous is False
    assert "test.unspecified_sync" in caplog.text
    assert "missing the synchronous= parameter" in caplog.text


@pytest.mark.parametrize("capture", ["", "="])
@pytest.mark.parametrize("with_template_arg", [True, False])
def test_new_lambda_pvariable(capture: str, with_template_arg: bool) -> None:
    lambda_expr = LambdaExpression(["return true;"], [], capture)
    id_obj = ID("lam", is_declaration=True, type=LambdaCondition)
    new_lambda_pvariable(
        id_obj,
        lambda_expr,
        StatelessLambdaCondition,
        TEMPLATE_ARG if with_template_arg else None,
    )
    text = CORE.cpp_main_section
    expected_class = "StatelessLambdaCondition" if capture == "" else "LambdaCondition"
    assert f"new(lam) {expected_class}" in text
    assert ("<>(" in text) == with_template_arg
    # The caller's id is left alone
    assert id_obj.type is LambdaCondition


def test_validate_potentially_or_condition_single() -> None:
    result = validate_potentially_or_condition({"lambda": Lambda("return true;")})
    assert "lambda" in result


def test_validate_potentially_or_condition_list() -> None:
    result = validate_potentially_or_condition([{"lambda": Lambda("return true;")}])
    assert "or" in result


def test_validate_automation_list_of_invalid_actions_reports_both() -> None:
    with pytest.raises(cv.MultipleInvalid) as exc_info:
        validate_automation()([1])
    messages = [str(err) for err in exc_info.value.errors]
    assert any("Action must consist of key-value mapping" in m for m in messages)
    assert any("expected a dictionary" in m for m in messages)


def test_validate_automation_list_unknown_action_reports_automation_error() -> None:
    with pytest.raises(cv.Invalid, match="no_such_action"):
        validate_automation()([{CONF_THEN: [{"no_such_action": {}}]}])


def test_validate_automation_non_list_non_dict() -> None:
    with pytest.raises(cv.Invalid):
        validate_automation()("not an automation")


def test_validate_automation_schema_extract() -> None:
    schema = validate_automation({cv.Optional("extra"): cv.string})(SCHEMA_EXTRACT)
    assert "extra" in [str(key) for key in schema.schema]


def test_validate_automation_single_rejects_multiple() -> None:
    with pytest.raises(cv.Invalid, match="only a single automation"):
        validate_automation(single=True)(
            [{CONF_THEN: [{"delay": "1s"}]}, {CONF_THEN: [{"delay": "2s"}]}]
        )


def _lambda_condition(id_: str) -> ConfigType:
    return {
        "lambda": Lambda("return true;"),
        CONF_TYPE_ID: ID(id_, is_declaration=True, type=LambdaCondition),
    }


@pytest.mark.asyncio
async def test_not_condition_wraps_inner_condition() -> None:
    CORE.config = {}
    config: ConfigType = {
        "not": _lambda_condition("inner"),
        CONF_TYPE_ID: ID("outer", is_declaration=True, type=NotCondition),
    }
    await build_condition(config, TEMPLATE_ARG, [])
    assert "new(outer) NotCondition<>(inner);" in CORE.cpp_main_section


@pytest.mark.asyncio
async def test_for_condition_sets_time() -> None:
    CORE.config = {}
    CORE.component_ids.add("for_cond")
    config: ConfigType = {
        "for": {
            CONF_TIME: 1000,
            CONF_CONDITION: _lambda_condition("inner"),
        },
        CONF_TYPE_ID: ID("for_cond", is_declaration=True, type=ForCondition),
    }
    await build_condition(config, TEMPLATE_ARG, [])
    assert "for_cond->set_time(" in CORE.cpp_main_section


@pytest.mark.asyncio
@pytest.mark.parametrize("with_timeout", [True, False])
async def test_wait_until_action_timeout_is_optional(with_timeout: bool) -> None:
    CORE.config = {}
    CORE.component_ids.add("wait")
    body: ConfigType = {CONF_CONDITION: _lambda_condition("inner")}
    if with_timeout:
        body[CONF_TIMEOUT] = 500
    config: ConfigType = {
        "wait_until": body,
        CONF_TYPE_ID: ID("wait", is_declaration=True, type=WaitUntilAction),
    }
    await build_action(config, TEMPLATE_ARG, [])
    assert ("wait->set_timeout_value(" in CORE.cpp_main_section) == with_timeout


@pytest.mark.asyncio
@pytest.mark.parametrize("with_interval", [True, False])
async def test_component_resume_action_update_interval_is_optional(
    with_interval: bool,
) -> None:
    CORE.config = {}
    cg.new_Pvariable(ID("comp", is_declaration=True, type=cg.PollingComponent))
    body: ConfigType = {CONF_ID: ID("comp", type=cg.PollingComponent)}
    if with_interval:
        body[CONF_UPDATE_INTERVAL] = 250
    config: ConfigType = {
        "component.resume": body,
        CONF_TYPE_ID: ID("resume", is_declaration=True, type=ResumeComponentAction),
    }
    await build_action(config, TEMPLATE_ARG, [])
    assert ("resume->set_update_interval(" in CORE.cpp_main_section) == with_interval


COMPONENT_ID = ID("comp", type=cg.Component)


def _action(name: str, id_: str, is_manual: bool = False) -> ConfigType:
    """Config for an action taking the component id; ``id_`` names the action object."""
    return {
        name: {CONF_ID: COMPONENT_ID},
        CONF_TYPE_ID: ID(
            id_, is_declaration=True, type=ApplyAction, is_manual=is_manual
        ),
    }


def _lambda_action(id_: str, body: str) -> ConfigType:
    return {
        "lambda": Lambda(body),
        CONF_TYPE_ID: ID(id_, is_declaration=True, type=LambdaAction, is_manual=False),
    }


@pytest.fixture
def apply_actions() -> Generator[None]:
    """Declare the component and register apply actions ``my.a`` to ``my.c``."""
    CORE.config = {}
    cg.new_Pvariable(ID("comp", is_declaration=True, type=cg.Component))
    schema = {cv.Required(CONF_ID): cv.use_id(cg.Component)}
    actions = Registry()
    actions.update(ACTION_REGISTRY)
    with patch("esphome.automation.ACTION_REGISTRY", actions):
        for name in ("a", "b", "c"):
            register_apply_action(f"my.{name}", schema, ApplyCall(f"do_{name}()"))
        yield


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_merges_adjacent_actions() -> None:
    """Adjacent inline actions become one object, with a stop check between the actions."""
    config = [_action("my.a", "act_a"), _action("my.b", "act_b")]

    result = await build_action_list(config, TEMPLATE_ARG, [])

    assert len(result) == 1
    assert "new(act_a) ApplyRunAction<esphome__act_a__fn>();" in CORE.cpp_main_section
    assert "act_b" not in CORE.cpp_main_section
    text = CORE.cpp_global_section
    assert "void esphome__act_a__fn(const int & esphome__running)" in text
    assert "esphome__act_b__fn" not in text
    assert text.index("::comp->do_a();") < text.index("if (!esphome__running)")
    assert text.index("if (!esphome__running)") < text.index("::comp->do_b();")
    assert text.count("if (!esphome__running)") == 1


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_single_action_is_unchanged() -> None:
    """A lone apply action keeps its plain ApplyAction and no stop check."""
    result = await build_action_list([_action("my.a", "act_a")], TEMPLATE_ARG, [])

    assert len(result) == 1
    assert "new(act_a) ApplyAction<esphome__act_a__fn>();" in CORE.cpp_main_section
    assert "esphome__running" not in CORE.cpp_global_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_single_lambda_is_unchanged() -> None:
    """A lone lambda keeps the StatelessLambdaAction that shares one vtable."""
    config = [_lambda_action("act_l", "do_it();")]

    result = await build_action_list(config, TEMPLATE_ARG, [])

    assert len(result) == 1
    assert "new(act_l) StatelessLambdaAction<>(" in CORE.cpp_main_section
    assert "esphome__running" not in CORE.cpp_global_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_merges_lambda_as_immediate_call() -> None:
    """A lambda keeps its scope and argument copy; a leading one must not type the object."""
    config = [_lambda_action("act_l", "x += 1;\nreturn;"), _action("my.a", "act_a")]
    args = [(cg.int_, "x")]

    result = await build_action_list(config, cg.TemplateArguments(cg.int_), args)

    assert len(result) == 1
    assert "new(act_l) ApplyRunAction<esphome__act_l__fn, int>();" in (
        CORE.cpp_main_section
    )
    text = CORE.cpp_global_section
    assert "const std::remove_cvref_t<int> & x" in text
    assert "[](int x) -> void {" in text
    assert "}(x);" in text


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_object_action_ends_a_run() -> None:
    """An action with its own class splits the runs on either side of it."""
    delay = {
        "delay": 100,
        CONF_TYPE_ID: ID("act_d", is_declaration=True, type=DelayAction),
    }
    config = [
        _action("my.a", "act_a"),
        _action("my.b", "act_b"),
        delay,
        _action("my.c", "act_c"),
    ]

    result = await build_action_list(config, TEMPLATE_ARG, [])

    assert [str(action) for action in result] == ["act_a", "act_d", "act_c"]
    text = CORE.cpp_main_section
    assert "new(act_a) ApplyRunAction<esphome__act_a__fn>();" in text
    assert "new(act_c) ApplyAction<esphome__act_c__fn>();" in text


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_manual_type_id_keeps_its_object() -> None:
    """A lambda can reach an action by a manual type_id, so it stays a separate object."""
    config = [
        _action("my.a", "act_a"),
        _action("my.b", "act_b"),
        _action("my.c", "act_c", is_manual=True),
    ]

    result = await build_action_list(config, TEMPLATE_ARG, [])

    assert [str(action) for action in result] == ["act_a", "act_c"]
    assert "new(act_c) ApplyAction<esphome__act_c__fn>();" in CORE.cpp_main_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_keeps_wrapper_side_effects() -> None:
    """A builder that wraps an apply builder still runs, and its action still merges."""
    builder = apply_action_builder(
        "my.wrapped",
        {cv.Required(CONF_ID): cv.use_id(cg.Component)},
        ApplyCall("do_wrapped()"),
    )

    @register_action("my.wrapped", ApplyAction, {}, synchronous=True)
    async def wrapped_to_code(
        config: ConfigType,
        action_id: ID,
        template_arg: cg.TemplateArguments,
        args: list[tuple[object, str]],
    ) -> MockObj:
        cg.add_define("USE_MY_WRAPPED")
        return await builder(config, action_id, template_arg, args)

    config = [_action("my.wrapped", "act_w"), _action("my.a", "act_a")]

    result = await build_action_list(config, TEMPLATE_ARG, [])

    assert len(result) == 1
    assert any(define.name == "USE_MY_WRAPPED" for define in CORE.defines)
    assert "::comp->do_wrapped();" in CORE.cpp_global_section


def _inline_condition(id_: str = "cond") -> ConfigType:
    return {
        "lambda": Lambda("return true;"),
        CONF_TYPE_ID: ID(
            id_, is_declaration=True, type=LambdaCondition, is_manual=False
        ),
    }


def _if_action(*then: ConfigType, condition: ConfigType | None = None) -> ConfigType:
    return {
        "if": {
            CONF_CONDITION: condition or _inline_condition(),
            CONF_THEN: list(then),
        },
        CONF_TYPE_ID: ID("act_if", is_declaration=True, type=IfAction, is_manual=False),
    }


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_rejects_a_dropped_collected_action() -> None:
    """A wrapper that returns another object would lose the collected action without a trace."""
    builder = apply_action_builder(
        "my.wrapped",
        {cv.Required(CONF_ID): cv.use_id(cg.Component)},
        ApplyCall("do_wrapped()"),
    )

    @register_action("my.wrapped", ApplyAction, {}, synchronous=True)
    async def wrapped_to_code(
        config: ConfigType,
        action_id: ID,
        template_arg: cg.TemplateArguments,
        args: list[tuple[object, str]],
    ) -> MockObj:
        await builder(config, action_id, template_arg, args)
        return MockObj("something_else", "->")

    with pytest.raises(
        EsphomeError, match=r"'my\.wrapped' \(act_w\) must return the result"
    ):
        await build_action_list([_action("my.wrapped", "act_w")], TEMPLATE_ARG, [])


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_rejects_using_the_collected_result() -> None:
    """A wrapper that uses the result fails here, not later as an undeclared name in C++."""
    builder = apply_action_builder(
        "my.wrapped",
        {cv.Required(CONF_ID): cv.use_id(cg.Component)},
        ApplyCall("do_wrapped()"),
    )

    @register_action("my.wrapped", ApplyAction, {}, synchronous=True)
    async def wrapped_to_code(
        config: ConfigType,
        action_id: ID,
        template_arg: cg.TemplateArguments,
        args: list[tuple[object, str]],
    ) -> MockObj:
        var = await builder(config, action_id, template_arg, args)
        cg.add(var.set_x(1))
        return var

    with pytest.raises(EsphomeError, match=r"'my\.wrapped' \(act_w\) used the result"):
        await build_action_list([_action("my.wrapped", "act_w")], TEMPLATE_ARG, [])


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_rejects_building_an_action_twice() -> None:
    """The second build would otherwise replace the first without a trace."""
    builder = apply_action_builder(
        "my.wrapped",
        {cv.Required(CONF_ID): cv.use_id(cg.Component)},
        ApplyCall("do_wrapped()"),
    )

    @register_action("my.wrapped", ApplyAction, {}, synchronous=True)
    async def wrapped_to_code(
        config: ConfigType,
        action_id: ID,
        template_arg: cg.TemplateArguments,
        args: list[tuple[object, str]],
    ) -> MockObj:
        await builder(config, action_id, template_arg, args)
        return await builder(config, action_id, template_arg, args)

    with pytest.raises(
        EsphomeError, match=r"'my\.wrapped' \(act_w\) built its action twice"
    ):
        await build_action_list([_action("my.wrapped", "act_w")], TEMPLATE_ARG, [])


@pytest.mark.parametrize(
    "use",
    [str, lambda result: result(), lambda result: result.set_x],
    ids=["str", "call", "attribute"],
)
def test_collected_result_cannot_be_used(use: Callable[[MockObj], object]) -> None:
    with pytest.raises(EsphomeError, match="The builder of an action used the result"):
        use(_COLLECTED_ACTION)


def test_collected_result_leaves_python_protocols_alone() -> None:
    """Python probes for dunder methods and expects them to be missing."""
    assert not hasattr(_COLLECTED_ACTION, "__wrapped__")


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_inline_if_is_control_flow() -> None:
    """An all-inline ``if`` merges with its neighbours and checks for a stop after the condition."""
    config = [
        _action("my.c", "act_c"),
        _if_action(_action("my.a", "act_a"), _action("my.b", "act_b")),
    ]

    result = await build_action_list(config, TEMPLATE_ARG, [])

    assert [str(action) for action in result] == ["act_c"]
    main = CORE.cpp_main_section
    assert "new(act_c) ApplyRunAction<esphome__act_c__fn>();" in main
    assert "IfAction" not in main
    assert "Condition" not in main
    text = CORE.cpp_global_section
    assert "const bool esphome__condition = ([]() -> bool {" in text
    # Between the two outer actions, after the condition and between the two branch actions
    assert text.count("if (!esphome__running)") == 3


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_single_inline_if_has_stop_checks() -> None:
    """Alone, control flow still needs a run object, because its condition can stop it."""
    result = await build_action_list(
        [_if_action(_action("my.a", "act_a"))], TEMPLATE_ARG, []
    )

    assert [str(action) for action in result] == ["act_if"]
    assert "new(act_if) ApplyRunAction<esphome__act_if__fn>();" in (
        CORE.cpp_main_section
    )


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_build_action_list_if_with_object_branch_keeps_objects() -> None:
    """An ``if`` with a deferring action keeps its object and condition."""
    delay = {
        "delay": 100,
        CONF_TYPE_ID: ID("act_d", is_declaration=True, type=DelayAction),
    }
    config = [_if_action(_action("my.a", "act_a"), _action("my.b", "act_b"), delay)]

    result = await build_action_list(config, TEMPLATE_ARG, [])

    assert [str(action) for action in result] == ["act_if"]
    text = CORE.cpp_main_section
    assert "new(cond) StatelessLambdaCondition<>(" in text
    assert "new(act_a) ApplyRunAction<esphome__act_a__fn>();" in text
    assert "act_if->add_then_action(act_a);" in text
    assert "act_if->add_then_action(act_d);" in text
    assert "act_if->finish_then();" in text


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
@pytest.mark.parametrize(
    ("key", "type_", "expected"),
    [
        ("and", AndCondition, ") && ("),
        ("or", OrCondition, ") || ("),
    ],
    ids=["and", "or"],
)
async def test_inline_condition_group_operators(
    key: str, type_: cg.MockObjClass, expected: str
) -> None:
    """Each condition group becomes its operator."""
    group = {
        key: [_inline_condition("c_1"), _inline_condition("c_2")],
        CONF_TYPE_ID: ID("group", is_declaration=True, type=type_, is_manual=False),
    }
    config = [_if_action(_action("my.a", "act_a"), condition=group)]

    await build_action_list(config, TEMPLATE_ARG, [])

    assert expected in _squash(CORE.cpp_global_section)
    assert "Condition" not in CORE.cpp_main_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_xor_of_several_stays_an_object() -> None:
    """C++ does not define the order of ``+`` operands, and xor evaluates in order."""
    group = {
        "xor": [_inline_condition("c_1"), _inline_condition("c_2")],
        CONF_TYPE_ID: ID(
            "group", is_declaration=True, type=XorCondition, is_manual=False
        ),
    }

    await build_action_list(
        [_if_action(_action("my.a", "act_a"), condition=group)], TEMPLATE_ARG, []
    )

    assert "new(group) XorCondition<2>(c_1, c_2);" in CORE.cpp_main_section
    assert "::group->check()" in CORE.cpp_global_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_xor_of_one_is_that_condition() -> None:
    group = {
        "xor": [_inline_condition("c_1")],
        CONF_TYPE_ID: ID(
            "group", is_declaration=True, type=XorCondition, is_manual=False
        ),
    }

    await build_action_list(
        [_if_action(_action("my.a", "act_a"), condition=group)], TEMPLATE_ARG, []
    )

    assert "Condition" not in CORE.cpp_main_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_new_lambda_pvariable_keeps_other_stateless_classes() -> None:
    """A builder using its own stateless class keeps that object, and ends the run."""
    my_action = cg.esphome_ns.class_("MyStatelessAction", Action)

    @register_action("my.stateless", my_action, {}, synchronous=True)
    async def stateless_to_code(
        config: ConfigType,
        action_id: ID,
        template_arg: cg.TemplateArguments,
        args: list[tuple[object, str]],
    ) -> MockObj:
        lambda_ = await cg.process_lambda(Lambda("do_it();"), args, return_type=cg.void)
        return new_lambda_pvariable(action_id, lambda_, my_action, template_arg)

    config = [
        {
            "my.stateless": {},
            CONF_TYPE_ID: ID(
                "act_m", is_declaration=True, type=my_action, is_manual=False
            ),
        }
    ]

    result = await build_action_list(config, TEMPLATE_ARG, [])

    assert [str(action) for action in result] == ["act_m"]
    assert "new(act_m) MyStatelessAction<>(" in CORE.cpp_main_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_condition_builder_must_return_collected_condition() -> None:
    """A condition builder that drops what it collected fails during code generation."""
    builder_schema = {cv.Required(CONF_ID): cv.use_id(cg.Component)}
    conditions = Registry()
    with patch("esphome.automation.CONDITION_REGISTRY", conditions):
        register_apply_condition("my.real", builder_schema, "is_ready()")
        real = conditions["my.real"].fun

        @register_condition("my.drops", ApplyCondition, builder_schema)
        async def drops_to_code(
            config: ConfigType,
            condition_id: ID,
            template_arg: cg.TemplateArguments,
            args: list[tuple[object, str]],
        ) -> MockObj:
            await real(config, condition_id, template_arg, args)
            return MockObj("something_else", "->")

        condition = {
            "my.drops": {CONF_ID: COMPONENT_ID},
            CONF_TYPE_ID: ID(
                "cond", is_declaration=True, type=ApplyCondition, is_manual=False
            ),
        }

        with pytest.raises(
            EsphomeError, match=r"'my\.drops' \(cond\) must return the result"
        ):
            await build_action_list(
                [_if_action(_action("my.a", "act_a"), condition=condition)],
                TEMPLATE_ARG,
                [],
            )


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
@pytest.mark.parametrize(
    "statements",
    [
        ["first();", "return second();"],
        ["first(); return second();"],
        ["ok = check();"],
        ["return check()"],
        [],
    ],
    ids=["two_entries", "two_in_one", "not_a_return", "no_semicolon", "empty"],
)
async def test_condition_must_generate_one_return_statement(
    statements: list[str],
) -> None:
    """Anything else cannot become an expression, and could still compile as one."""
    conditions = Registry()
    with patch("esphome.automation.CONDITION_REGISTRY", conditions):

        @register_condition("my.bad", ApplyCondition, {})
        async def bad_to_code(
            config: ConfigType,
            condition_id: ID,
            template_arg: cg.TemplateArguments,
            args: list[tuple[object, str]],
        ) -> MockObj:
            return apply_statements_action(condition_id, template_arg, args, statements)

        condition = {
            "my.bad": {},
            CONF_TYPE_ID: ID(
                "cond", is_declaration=True, type=ApplyCondition, is_manual=False
            ),
        }

        with pytest.raises(
            EsphomeError, match=r"'my\.bad' \(cond\) must generate one 'return"
        ):
            await build_action_list(
                [_if_action(_action("my.a", "act_a"), condition=condition)],
                TEMPLATE_ARG,
                [],
            )


@pytest.mark.asyncio
async def test_condition_group_outside_control_flow_is_an_object() -> None:
    """A group built on its own, as ``wait_until`` does, stays an object with its children."""
    CORE.config = {}
    group = {
        "and": [_inline_condition("c_1"), _inline_condition("c_2")],
        CONF_TYPE_ID: ID(
            "group", is_declaration=True, type=AndCondition, is_manual=False
        ),
    }

    result = await build_condition(group, TEMPLATE_ARG, [])

    assert str(result) == "group"
    assert "new(group) AndCondition<2>(c_1, c_2);" in CORE.cpp_main_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_empty_condition_group_is_called_through_its_object() -> None:
    """An empty group has no expression to join, so ``if`` calls its object."""
    group = {
        "or": [],
        CONF_TYPE_ID: ID(
            "group", is_declaration=True, type=OrCondition, is_manual=False
        ),
    }

    await build_action_list(
        [_if_action(_action("my.a", "act_a"), condition=group)], TEMPLATE_ARG, []
    )

    assert "new(group) OrCondition<0>();" in CORE.cpp_main_section
    assert "::group->check()" in CORE.cpp_global_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_inline_not_condition() -> None:
    """``not`` negates its child without an object."""
    negated = {
        "not": _inline_condition("inner"),
        CONF_TYPE_ID: ID(
            "neg", is_declaration=True, type=NotCondition, is_manual=False
        ),
    }
    config = [_if_action(_action("my.a", "act_a"), condition=negated)]

    await build_action_list(config, TEMPLATE_ARG, [])

    assert "(!([]() -> bool {" in CORE.cpp_global_section
    assert "new(neg)" not in CORE.cpp_main_section
    assert "new(inner)" not in CORE.cpp_main_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_not_condition_is_an_object_when_its_if_keeps_one() -> None:
    """A deferring action in the branch makes the collected ``not`` and its child emit objects."""
    negated = {
        "not": _inline_condition("inner"),
        CONF_TYPE_ID: ID(
            "neg", is_declaration=True, type=NotCondition, is_manual=False
        ),
    }
    config = [_if_action(_delay("act_d"), condition=negated)]

    await build_action_list(config, TEMPLATE_ARG, [])

    main = CORE.cpp_main_section
    assert "new(inner) StatelessLambdaCondition<>(" in main
    assert "new(neg) NotCondition<>(inner);" in main
    assert "new(act_if) IfAction<false>(neg);" in main


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_inline_while_checks_condition_and_stops() -> None:
    config = [
        {
            "while": {
                CONF_CONDITION: _inline_condition(),
                CONF_THEN: [_action("my.a", "act_a")],
            },
            CONF_TYPE_ID: ID(
                "act_w", is_declaration=True, type=WhileAction, is_manual=False
            ),
        }
    ]

    result = await build_action_list(config, TEMPLATE_ARG, [])

    assert [str(action) for action in result] == ["act_w"]
    assert "WhileAction" not in CORE.cpp_main_section
    text = CORE.cpp_global_section
    assert "while (true) {" in text
    assert "if (!esphome__condition)\n" in text
    assert text.count("if (!esphome__running)") == 2


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_inline_while_with_empty_body_keeps_its_object() -> None:
    """An empty body exits a WhileAction at once, which a C++ loop would not do."""
    config = [
        {
            "while": {CONF_CONDITION: _inline_condition(), CONF_THEN: []},
            CONF_TYPE_ID: ID(
                "act_w", is_declaration=True, type=WhileAction, is_manual=False
            ),
        }
    ]

    await build_action_list(config, TEMPLATE_ARG, [])

    assert "new(act_w) WhileAction<>(" in CORE.cpp_main_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_inline_repeat_rereads_count_and_aliases_outer_iteration() -> None:
    """The count is in the loop condition, and an outer iteration keeps its value."""
    config = [
        {
            "repeat": {
                CONF_COUNT: 3,
                CONF_THEN: [_lambda_action("act_l", "use(iteration_1);")],
            },
            CONF_TYPE_ID: ID(
                "act_r", is_declaration=True, type=RepeatAction, is_manual=False
            ),
        }
    ]
    args = [(cg.uint32, "iteration")]

    result = await build_action_list(config, cg.TemplateArguments(cg.uint32), args)

    assert [str(action) for action in result] == ["act_r"]
    assert "RepeatAction" not in CORE.cpp_main_section
    text = CORE.cpp_global_section
    assert "const auto &iteration_1 = iteration;" in text
    assert "esphome__iteration < [](uint32_t iteration) -> uint32_t {" in text
    assert "const uint32_t iteration = esphome__iteration;" in text
    assert text.index("iteration_1 = iteration") < text.index("for (uint32_t")


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_inline_if_with_manual_condition_id_calls_the_object() -> None:
    """A lambda can reach a condition by a manual id, so it keeps its object."""
    condition = {
        "lambda": Lambda("return true;"),
        CONF_TYPE_ID: ID(
            "my_cond", is_declaration=True, type=LambdaCondition, is_manual=True
        ),
    }

    await build_action_list(
        [_if_action(_action("my.a", "act_a"), condition=condition)], TEMPLATE_ARG, []
    )

    assert "new(my_cond) StatelessLambdaCondition<>(" in CORE.cpp_main_section
    assert "::my_cond->check()" in CORE.cpp_global_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_callback_automation_inline_if_drops_stop_checks() -> None:
    """Nothing can stop a direct automation, so its control flow has no stop checks."""
    conf = _automation_conf(
        _if_action(_action("my.a", "act_a"), _action("my.b", "act_b"))
    )

    await build_callback_automation(CALLBACK_PARENT, "add_cb", [], conf)

    assert "Action" not in CORE.cpp_main_section
    text = CORE.cpp_global_section
    assert "if (esphome__condition) {" in text
    assert "esphome__running" not in text


def _choose_action(*choices: ConfigType) -> ConfigType:
    return {
        CONF_CHOOSE: list(choices),
        CONF_TYPE_ID: ID("act_ch", is_declaration=True, type=IfAction, is_manual=False),
    }


def _choice(id_: str, *then: ConfigType, is_manual: bool = False) -> ConfigType:
    return {
        "if": _inline_condition(f"cond_{id_}"),
        CONF_THEN: list(then),
        CONF_CHOICE_ID: ID(
            id_, is_declaration=True, type=IfAction, is_manual=is_manual
        ),
    }


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_inline_choose_is_nested_if_else() -> None:
    config = [
        _choose_action(
            _choice("ch_a", _action("my.a", "act_a")),
            _choice("ch_b", _action("my.b", "act_b")),
            {
                CONF_ELSE: [_action("my.c", "act_c")],
                CONF_CHOICE_ID: ID("ch_c", is_declaration=True, is_manual=False),
            },
        )
    ]

    result = await build_action_list(config, TEMPLATE_ARG, [])

    assert [str(action) for action in result] == ["act_ch"]
    assert "IfAction" not in CORE.cpp_main_section
    text = CORE.cpp_global_section
    assert text.index("::comp->do_a();") < text.index("} else {")
    assert text.index("::comp->do_a();") < text.index("::comp->do_b();")
    assert text.index("::comp->do_b();") < text.index("::comp->do_c();")
    assert text.count("} else {") == 2


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_choose_manual_choice_id_keeps_objects() -> None:
    """A lambda can reach a choice by a manual id, so the chain stays objects."""
    config = [
        _choose_action(
            _choice("ch_a", _action("my.a", "act_a")),
            _choice("ch_b", _action("my.b", "act_b"), is_manual=True),
        )
    ]

    await build_action_list(config, TEMPLATE_ARG, [])

    main = CORE.cpp_main_section
    assert "new(ch_b) IfAction<false>(" in main
    assert "new(act_ch) IfAction<true>(" in main


def _automation_conf(*then: ConfigType, is_manual: bool = False) -> ConfigType:
    return {
        CONF_THEN: list(then),
        CONF_AUTOMATION_ID: ID(
            "auto", is_declaration=True, type=Automation, is_manual=is_manual
        ),
    }


def _delay(id_: str) -> ConfigType:
    return {
        "delay": 100,
        CONF_TYPE_ID: ID(id_, is_declaration=True, type=DelayAction),
    }


CALLBACK_PARENT = MockObj("parent", "->")


@pytest.mark.asyncio
@pytest.mark.parametrize("forwarder", [TriggerOnTrueForwarder, TriggerOnFalseForwarder])
async def test_callback_automation_core_forwarder_rejects_args(
    forwarder: MockObj,
) -> None:
    """These forwarders call an Automation<>, so arguments would only fail in the C++ build."""
    conf = _automation_conf()

    with pytest.raises(ValueError, match="no arguments"):
        await build_callback_automation(
            CALLBACK_PARENT, "add_cb", [(cg.bool_, "x")], conf, forwarder=forwarder
        )


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_callback_automation_all_inline_has_no_objects() -> None:
    """A callback runs an all-inline automation directly, with no objects or stop checks."""
    conf = _automation_conf(_action("my.a", "act_a"), _action("my.b", "act_b"))

    await build_callback_automation(CALLBACK_PARENT, "add_cb", [(cg.bool_, "x")], conf)

    main = CORE.cpp_main_section
    assert "Automation" not in main
    assert "Action" not in main
    assert "parent->add_cb([](const std::remove_cvref_t<bool> & x) -> void {" in main
    assert "esphome__act_a__fn(x);" in main
    text = CORE.cpp_global_section
    assert "::comp->do_a();" in text
    assert "::comp->do_b();" in text
    assert "esphome__running" not in text


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
@pytest.mark.parametrize(
    ("forwarder", "check"),
    [
        (TriggerOnTrueForwarder, "if (!(state))"),
        (TriggerOnFalseForwarder, "if (!(!state))"),
    ],
)
async def test_callback_automation_core_forwarder_becomes_filter(
    forwarder: MockObj, check: str
) -> None:
    """The on-true and on-false forwarders turn into the same filter in the lambda."""
    conf = _automation_conf(_action("my.a", "act_a"))

    await build_callback_automation(
        CALLBACK_PARENT, "add_cb", [], conf, forwarder=forwarder
    )

    main = CORE.cpp_main_section
    assert "Forwarder" not in main
    assert "Automation" not in main
    assert check in main
    assert "esphome__act_a__fn();" in main


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_callback_automation_unknown_forwarder_keeps_objects() -> None:
    """A forwarder alone may filter in ways codegen cannot see, so it is kept."""
    forwarder = cg.esphome_ns.class_("MyForwarder")
    conf = _automation_conf(_action("my.a", "act_a"))

    await build_callback_automation(
        CALLBACK_PARENT, "add_cb", [], conf, forwarder=forwarder
    )

    main = CORE.cpp_main_section
    assert "new(auto) Automation<>();" in main
    assert "new(act_a) ApplyAction<esphome__act_a__fn>();" in main
    assert "parent->add_cb(MyForwarder{auto})" in main


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_callback_automation_manual_id_keeps_objects() -> None:
    """A lambda can reach an automation by a manual id, so it needs its objects."""
    conf = _automation_conf(_action("my.a", "act_a"), is_manual=True)

    await build_callback_automation(CALLBACK_PARENT, "add_cb", [], conf)

    main = CORE.cpp_main_section
    assert "new(auto) Automation<>();" in main
    assert "parent->add_cb(TriggerForwarder<>{auto})" in main


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_callback_automation_with_object_action_keeps_objects() -> None:
    """A deferring action needs the chain, so the inline actions stay objects around it."""
    conf = _automation_conf(
        _action("my.a", "act_a"), _delay("act_d"), _action("my.b", "act_b")
    )

    await build_callback_automation(CALLBACK_PARENT, "add_cb", [], conf)

    main = CORE.cpp_main_section
    assert "new(auto) Automation<>();" in main
    assert "auto->add_action(act_a);" in main
    assert "auto->add_action(act_d);" in main
    assert "auto->add_action(act_b);" in main
    assert "parent->add_cb(TriggerForwarder<>{auto})" in main


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions")
async def test_trigger_callback_all_inline_is_direct() -> None:
    """Direct callers of build_trigger_callback get the direct call too."""
    conf = _automation_conf(_action("my.a", "act_a"), _action("my.b", "act_b"))

    callback = await build_trigger_callback(
        [(cg.int_, "x")], conf, [(cg.int_, "index")], forward=["index + 1"]
    )

    assert "esphome__act_a__fn(index + 1);" in str(callback)
    assert "Automation" not in CORE.cpp_main_section


@pytest.fixture
def statements_action() -> Generator[None]:
    """Register ``my.statements``, a builder that generates its own statements."""

    @register_action("my.statements", ApplyAction, {}, synchronous=True)
    async def statements_to_code(
        config: ConfigType,
        action_id: ID,
        template_arg: cg.TemplateArguments,
        args: list[tuple[object, str]],
    ) -> MockObj:
        return apply_statements_action(
            action_id, template_arg, args, ["auto esphome__v = x;", "use(esphome__v);"]
        )

    yield


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions", "statements_action")
async def test_apply_statements_action_alone_is_an_apply_action() -> None:
    """Alone, it is a plain ApplyAction taking the arguments by const reference."""
    config = [_action("my.statements", "act_s")]

    result = await build_action_list(
        config, cg.TemplateArguments(cg.int_), [(cg.int_, "x")]
    )

    assert [str(action) for action in result] == ["act_s"]
    assert "new(act_s) ApplyAction<esphome__act_s__fn, int>();" in CORE.cpp_main_section
    text = CORE.cpp_global_section
    assert "esphome__act_s__fn(const std::remove_cvref_t<int> & x)" in text
    assert "use(esphome__v);" in text


@pytest.mark.asyncio
@pytest.mark.usefixtures("apply_actions", "statements_action")
async def test_apply_statements_action_merges_with_neighbours() -> None:
    """Next to another inline action, its statements join the merged function."""
    config = [_action("my.statements", "act_s"), _action("my.a", "act_a")]

    result = await build_action_list(
        config, cg.TemplateArguments(cg.int_), [(cg.int_, "x")]
    )

    assert [str(action) for action in result] == ["act_s"]
    assert "ApplyRunAction<esphome__act_s__fn, int>" in CORE.cpp_main_section
    text = CORE.cpp_global_section
    assert text.index("use(esphome__v);") < text.index("::comp->do_a();")
