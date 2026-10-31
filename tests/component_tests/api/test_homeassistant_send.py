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


SEND = "api::send_homeassistant_fields(::api_apiserver_id, "


def test_action_without_response(main_cpp: str) -> None:
    """An action with no response handling is sent with the counts of each map."""
    assert f"{SEND}false, ::ha_action_fields, 1, 1, 1);" in main_cpp


def test_event_and_tag_scanned(main_cpp: str) -> None:
    """Events and scanned tags are sent as events."""
    assert f"{SEND}true, ::ha_action_fields_5, 0, 0, 0);" in main_cpp
    assert f"{SEND}true, ::ha_action_fields_6, 1, 0, 0);" in main_cpp
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
    """Script actions pass the script parameters to the field lambdas."""
    assert f"{SEND}false, ::ha_action_fields_2, 2, 0, 0, level);" in main_cpp
    assert f"{SEND}true, ::ha_action_fields_3, 1, 0, 0, level);" in main_cpp


def test_action_with_response_is_an_object(main_cpp: str) -> None:
    """on_error waits for the response, so the action keeps its object and trigger."""
    match = re.search(
        r"new\((\w+)\) api::HomeAssistantServiceCallAction<>"
        r"\(api_apiserver_id, ha_action_fields_4, 0, 0, 0\);",
        main_cpp,
    )
    assert match is not None
    action = match.group(1)
    assert f"Automation<std::string>({action}->get_error_trigger());" in main_cpp


def test_action_then_log(main_cpp: str) -> None:
    """The send is an if-free statement, so the action merged after it still runs."""
    match = re.search(
        r"static void (\w+)\(const int & esphome__running\) \{\n(.*?)\n\}\n",
        main_cpp,
        re.DOTALL,
    )
    assert match is not None
    body = match.group(2)
    assert body.index(f"{SEND}false, ::ha_action_fields,") < body.index(
        '"after action"'
    )
    # The only return is the stop check between the two actions
    assert body.count("return;") == 1
    assert re.findall(r"\bautomation_id->add_action\((\w+)\);", main_cpp) == [
        "applyaction_id"
    ]
    assert f"ApplyRunAction<{match.group(1)}>();" in main_cpp


def test_button_on_press(main_cpp: str) -> None:
    """A press callback that only sends calls one generated function, with no objects."""
    match = re.search(
        r"add_on_press_callback\(\[\]\(\) -> void \{\n    (\w+)\(\);\n\}\);",
        main_cpp,
    )
    assert match is not None
    body = main_cpp.split(f"static void {match.group(1)}() {{\n", 1)[1].split("\n}", 1)[
        0
    ]
    assert f"{SEND}true, ::ha_action_fields_5, 0, 0, 0);" in body
    assert f"{SEND}true, ::ha_action_fields_6, 1, 0, 0);" in body
    assert "TriggerForwarder" not in main_cpp
    assert "Automation<>()" not in main_cpp
