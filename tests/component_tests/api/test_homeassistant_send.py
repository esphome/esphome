"""Tests for the code generated for homeassistant.action, homeassistant.event and tag_scanned."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

CONFIG = "tests/component_tests/api/test_homeassistant_send.yaml"


@pytest.fixture(name="main_cpp")
def fixture_main_cpp(generate_main: Callable[[str | Path], str]) -> str:
    return generate_main(CONFIG)


def _table(main_cpp: str, name: str) -> str:
    match = re.search(rf"\b{name}\[\] PROGMEM = \{{\n(.*?)\n\}};", main_cpp, re.DOTALL)
    assert match is not None, name
    return match.group(1)


def test_field_order(main_cpp: str) -> None:
    """The name comes first, then data, data_template and variables, in that order."""
    table = _table(main_cpp, "ha_action_fields")
    assert re.fullmatch(
        r"  \{nullptr, progmem_str, nullptr\},\n"
        r"  \{progmem_str_2, progmem_str_3, nullptr\},\n"
        r"  \{progmem_str_4, progmem_str_5, nullptr\},\n"
        r"  api::HomeAssistantField<>::from_lambda\(progmem_str_6, \[\]\(\) \{\n"
        r".*return App\.get_name\(\);\n.*\n\}\),",
        table,
        re.DOTALL,
    ), table
    assert 'progmem_str[] PROGMEM = "notify.notify";' in main_cpp
    assert 'progmem_str_2[] PROGMEM = "message";' in main_cpp
    assert 'progmem_str_4[] PROGMEM = "title";' in main_cpp
    assert 'progmem_str_6[] PROGMEM = "name";' in main_cpp


def test_action_without_response(main_cpp: str) -> None:
    """An action with no response handling is sent with the counts of each map."""
    assert (
        "api::HomeAssistantServiceCallAction<>"
        "(api_apiserver_id, false, ha_action_fields, 1, 1, 1);"
    ) in main_cpp


def test_event_and_tag_scanned(main_cpp: str) -> None:
    """Events and scanned tags are sent as events."""
    assert (
        "api::HomeAssistantServiceCallAction<>"
        "(api_apiserver_id, true, ha_action_fields_5, 0, 0, 0);"
    ) in main_cpp
    assert (
        "api::HomeAssistantServiceCallAction<>"
        "(api_apiserver_id, true, ha_action_fields_6, 1, 0, 0);"
    ) in main_cpp
    assert "  {progmem_str_13, progmem_str_14, nullptr}," in _table(
        main_cpp, "ha_action_fields_6"
    )


@pytest.mark.parametrize(
    ("table", "body"),
    [
        ("ha_action_fields_2", "level *= 2; return level;"),
        ("ha_action_fields_2", "return level *= 3;"),
        ("ha_action_fields_3", "level *= 2; return level;"),
    ],
)
def test_lambdas_take_args_by_value(main_cpp: str, table: str, body: str) -> None:
    """A field lambda gets the trigger args by value, so its body may modify them."""
    assert re.search(
        r"from_lambda\(\w+, \[\]\(int level\) \{\n.*\n  " + re.escape(body),
        _table(main_cpp, table),
    )


def test_script_actions(main_cpp: str) -> None:
    """Script actions take the script parameters."""
    assert (
        "api::HomeAssistantServiceCallAction<int>"
        "(api_apiserver_id, false, ha_action_fields_2, 2, 0, 0);"
    ) in main_cpp
    assert (
        "api::HomeAssistantServiceCallAction<int>"
        "(api_apiserver_id, true, ha_action_fields_3, 1, 0, 0);"
    ) in main_cpp


def test_action_with_response_is_an_object(main_cpp: str) -> None:
    """on_error waits for the response, so the action keeps its object and trigger."""
    match = re.search(
        r"new\((\w+)\) api::HomeAssistantServiceCallAction<>"
        r"\(api_apiserver_id, false, ha_action_fields_4, 0, 0, 0\);",
        main_cpp,
    )
    assert match is not None
    action = match.group(1)
    assert f"{action}->set_wants_status();" in main_cpp
    assert f"Automation<std::string>({action}->get_error_trigger());" in main_cpp


def test_action_then_log(main_cpp: str) -> None:
    """The action sends before the action after it runs."""
    actions = re.findall(r"\bautomation_id->add_action\((\w+)\);", main_cpp)
    assert actions == ["api_homeassistantservicecallaction_id", "lambdaaction_id"]


def test_button_on_press(main_cpp: str) -> None:
    """A button sends its event and scanned tag from its press callback."""
    assert re.search(
        r"add_on_press_callback\(TriggerForwarder<>\{automation_id_5\}\);", main_cpp
    )
    sends = {
        match.group(2): match.group(1)
        for match in re.finditer(
            r"new\((\w+)\) api::HomeAssistantServiceCallAction<>"
            r"\(api_apiserver_id, true, (ha_action_fields_[56]), [01], 0, 0\);",
            main_cpp,
        )
    }
    actions = re.findall(r"\bautomation_id_5->add_action\((\w+)\);", main_cpp)
    assert actions == [sends["ha_action_fields_5"], sends["ha_action_fields_6"]]
