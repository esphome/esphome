"""Code generation of api user-defined actions (``api: actions:`` and legacy ``services:``).

Objects are found from the action name and the links between them, not from
auto-numbered ids, so adding or changing one action does not break the others.
"""

import re
from typing import NamedTuple

import pytest

from esphome.components.api import ACTIONS_SCHEMA
from esphome.config_validation import Invalid
from esphome.helpers import fnv1_hash

CONFIG = "tests/component_tests/api/test_user_services.yaml"
LEGACY_CONFIG = "tests/component_tests/api/test_user_services_legacy.yaml"

NONE = "api::enums::SUPPORTS_RESPONSE_NONE"
OPTIONAL = "api::enums::SUPPORTS_RESPONSE_OPTIONAL"
ONLY = "api::enums::SUPPORTS_RESPONSE_ONLY"
STATUS = "api::enums::SUPPORTS_RESPONSE_STATUS"
UNREGISTER = (
    "if (call_id != 0)\n    ::api_apiserver_id->unregister_active_action_call(call_id);"
)


class Service(NamedTuple):
    id: str
    type: str


def _constructed(main_cpp: str, id_: str) -> str:
    match = re.search(rf"\bnew\({id_}\) (.*?);\n", main_cpp, re.DOTALL)
    assert match is not None, id_
    return match.group(1)


def _actions_of(main_cpp: str, automation: str) -> list[str]:
    return re.findall(rf"\b{automation}->add_action\((\w+)\);", main_cpp)


def _function_body(main_cpp: str, name: str) -> str:
    match = re.search(
        rf"static void {name}\([^;{{]*\) \{{(.*?)\n\}}\n", main_cpp, re.DOTALL
    )
    assert match is not None, name
    return match.group(1)


def _strings(main_cpp: str, name: str) -> tuple[str, str]:
    """Return the id and the entries of the strings table of action ``name``."""
    match = re.search(
        rf'\b(api_action\d+_strings)\[\] PROGMEM = \{{("{name}"(?:, .*?)?)\}};',
        main_cpp,
    )
    assert match is not None, name
    return match.group(1), match.group(2)


def _service(main_cpp: str, name: str) -> Service:
    """Return the service object built from the strings table of action ``name``."""
    table, _ = _strings(main_cpp, name)
    match = re.search(
        rf"\bnew\((\w+)\) ([\w:]+<.*?>)\({table}, (\d+)(?:UL)?\);\n", main_cpp
    )
    assert match is not None, name
    assert int(match.group(3)) == fnv1_hash(name)
    return Service(match.group(1), match.group(2))


def _service_function(main_cpp: str, name: str, mode: str, types: str = "") -> str:
    """Check action ``name`` is a UserServiceCallback and return the function it calls."""
    service = _service(main_cpp, name)
    tail = f", {types}" if types else ""
    match = re.fullmatch(
        rf"api::UserServiceCallback<(\w+), {re.escape(mode + tail)}>", service.type
    )
    assert match is not None, service.type
    return match.group(1)


def _triggered_automation(main_cpp: str, function: str, args: str) -> str:
    """Check ``function`` only triggers an Automation with ``args``; return its id."""
    match = re.fullmatch(
        rf"\n  ::(\w+)->trigger\({re.escape(args)}\);",
        _function_body(main_cpp, function),
    )
    assert match is not None, function
    return match.group(1)


def _action_code(main_cpp: str, action: str) -> str:
    """Return the code an action runs: its generated function, or its constructor."""
    constructed = _constructed(main_cpp, action)
    if match := re.match(r"Apply(?:Run)?Action<(\w+)[,>]", constructed):
        return _function_body(main_cpp, match.group(1))
    return constructed


def _action_class(main_cpp: str, action: str) -> str:
    return re.match(r"[\w:]+", _constructed(main_cpp, action)).group(0)


def _assert_unregister_after(main_cpp: str, automation: str, types: str) -> None:
    """The last action of ``automation`` only unregisters the call."""
    last = _actions_of(main_cpp, automation)[-1]
    assert last == f"{automation}__after"
    assert _constructed(main_cpp, last) == (
        f"ApplyAction<esphome__{automation}__after__fn, {types}>()"
    )
    assert _action_code(main_cpp, last) == f"\n  {UNREGISTER}"


def test_no_args_service(generate_main) -> None:
    """Inline actions run in one function the service calls, with no Automation."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "no_args", NONE)
    body = _function_body(main_cpp, function)
    assert "NO_ARGS called" in body
    assert "::doubled->value() = 1;" in body
    assert "esphome__running" not in body
    assert "Automation<>" not in main_cpp
    # The service calls the function directly, with no action object around it
    assert f"Action<{function}" not in main_cpp


def test_mixed_args_types(generate_main) -> None:
    """Strings and arrays are non-owning views when every action is synchronous."""
    main_cpp = generate_main(CONFIG)
    types = (
        "bool, int32_t, float, StringRef, const FixedVector<int32_t> &, "
        "const FixedVector<std::string> &"
    )
    function = _service_function(main_cpp, "mixed_args", NONE, types)
    assert _strings(main_cpp, "mixed_args")[1] == (
        '"mixed_args", "bool_", "int_", "float_", "string_", "int_arr", "string_arr"'
    )
    # Arrays stay non-owning references in the generated function
    assert re.search(
        rf"static void {function}\([^)]*"
        r"const FixedVector<int32_t> & int_arr, "
        r"const FixedVector<std::string> & string_arr\) \{",
        main_cpp,
    )
    body = _function_body(main_cpp, function)
    assert "string_.c_str(), int_arr.size(), string_arr.size()" in body


def test_lambda_action_gets_args_by_value(generate_main) -> None:
    """A lambda action takes the arguments by value, so ``x *= 2`` compiles in both forms."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "by_value", NONE, "int32_t")
    body = _function_body(main_cpp, function)
    assert body.count("[](int32_t x) -> void {") == 2
    assert "x *= 2;" in body
    assert '"BY_VALUE again %d", x *= 2' in body


def test_delay_uses_owning_args(generate_main) -> None:
    """A delay keeps an Automation of separate actions, which the function triggers."""
    main_cpp = generate_main(CONFIG)
    types = "std::string, std::vector<int32_t>"
    function = _service_function(main_cpp, "with_delay", NONE, types)
    automation = _triggered_automation(main_cpp, function, "name, int_arr")
    assert _constructed(main_cpp, automation) == f"Automation<{types}>()"
    before, delay, after = _actions_of(main_cpp, automation)
    assert _action_class(main_cpp, delay) == "DelayAction"
    assert "WITH_DELAY before" in _action_code(main_cpp, before)
    after_code = _action_code(main_cpp, after)
    assert "WITH_DELAY %s %zu" in after_code
    assert "[](std::string name, std::vector<int32_t> int_arr) -> void" in after_code


def test_respond_optional(generate_main) -> None:
    """An optional response passes call_id and return_response; the call is unregistered last."""
    main_cpp = generate_main(CONFIG)
    types = "uint32_t, bool, int32_t"
    function = _service_function(main_cpp, "respond_optional", OPTIONAL, "int32_t")
    automation = _triggered_automation(
        main_cpp, function, "call_id, return_response, value"
    )
    assert _constructed(main_cpp, automation) == f"Automation<{types}>()"
    respond, _ = _actions_of(main_cpp, automation)
    assert _constructed(main_cpp, respond) == (
        f"api::APIRespondAction<{types}>(api_apiserver_id)"
    )
    assert f"{respond}->set_is_optional_mode(true);" in main_cpp
    _assert_unregister_after(main_cpp, automation, types)


def test_respond_only(generate_main) -> None:
    """A response-only service passes call_id; the call is unregistered after every action."""
    main_cpp = generate_main(CONFIG)
    types = "uint32_t, StringRef"
    function = _service_function(main_cpp, "respond_only", ONLY, "StringRef")
    automation = _triggered_automation(main_cpp, function, "call_id, name")
    assert _constructed(main_cpp, automation) == f"Automation<{types}>()"
    log, respond, _ = _actions_of(main_cpp, automation)
    assert "RESPOND_ONLY %s" in _action_code(main_cpp, log)
    assert _constructed(main_cpp, respond) == (
        f"api::APIRespondAction<{types}>(api_apiserver_id)"
    )
    assert f"{respond}->set_is_optional_mode" not in main_cpp
    _assert_unregister_after(main_cpp, automation, types)


def test_unregister_merges_with_trailing_inline_actions(generate_main) -> None:
    """After a non-inline action, the unregister joins the inline actions that follow it."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "respond_then_log", STATUS)
    automation = _triggered_automation(main_cpp, function, "call_id")
    respond, log = _actions_of(main_cpp, automation)
    assert _action_class(main_cpp, respond) == "api::APIRespondAction"
    body = _action_code(main_cpp, log)
    assert body.index("RESPONDED") < body.index("if (!esphome__running)")
    assert body.endswith(f"{UNREGISTER}\n  }}")


def test_unregister_in_fully_inline_response_service(generate_main) -> None:
    """With only inline actions, the unregister ends the function the service calls."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "status_inline", STATUS)
    body = _function_body(main_cpp, function)
    assert "->trigger(" not in body
    assert "esphome__running" not in body
    assert f"Action<{function}" not in main_cpp
    assert body.index("STATUS_INLINE first") < body.index("send_action_response")
    assert body.endswith(f"\n  {{\n  {UNREGISTER}\n  }}")


def test_manual_trigger_id(generate_main) -> None:
    """A trigger_id set by hand names the service object."""
    main_cpp = generate_main(CONFIG)
    service = _service(main_cpp, "manual_trigger_id")
    assert service.id == "manual_service_trigger"
    function = _service_function(main_cpp, "manual_trigger_id", NONE)
    assert "MANUAL called" in _function_body(main_cpp, function)


def test_lambda_uses_own_service_id(generate_main) -> None:
    """A lambda in a service can refer to the service's own id."""
    main_cpp = generate_main(CONFIG)
    service = _service(main_cpp, "self_reference")
    assert service.id == "self_reference_service"
    function = _service_function(main_cpp, "self_reference", NONE)
    body = _function_body(main_cpp, function)
    assert "(const void *) self_reference_service" in body


@pytest.mark.parametrize(
    ("mode", "name"),
    [
        ("status", "call_id"),
        ("optional", "call_id"),
        ("optional", "return_response"),
    ],
)
def test_reserved_variable_name_rejected(mode: str, name: str) -> None:
    """A variable cannot take the name of an argument the response mode passes first."""
    action = {"action": "x", "supports_response": mode, "variables": {name: "int"}}
    with pytest.raises(Invalid, match=f"'{name}' is reserved"):
        ACTIONS_SCHEMA({**action, "then": []})


@pytest.mark.parametrize(
    ("mode", "name"), [("none", "call_id"), ("status", "return_response")]
)
def test_variable_name_free_in_other_modes(mode: str, name: str) -> None:
    """The names are only reserved in the modes that pass those arguments."""
    action = {"action": "x", "supports_response": mode, "variables": {name: "int"}}
    (config,) = ACTIONS_SCHEMA({**action, "then": []})
    assert name in config["variables"]


def test_services_registered_in_order(generate_main) -> None:
    """Every service is registered with the server once, in config order."""
    main_cpp = generate_main(CONFIG)
    names = [
        "no_args",
        "mixed_args",
        "by_value",
        "with_delay",
        "respond_optional",
        "respond_only",
        "manual_trigger_id",
        "respond_then_log",
        "status_inline",
        "self_reference",
        "respond_named",
    ]
    match = re.search(
        r"\bapi_apiserver_id->initialize_user_services\(\{(.*?)\}\);", main_cpp
    )
    assert match is not None
    assert match.group(1).split(", ") == [_service(main_cpp, n).id for n in names]


def test_legacy_services_key(generate_main) -> None:
    """The legacy ``services:`` key with ``service:`` names builds the same service."""
    main_cpp = generate_main(LEGACY_CONFIG)
    assert _strings(main_cpp, "legacy_service")[1] == '"legacy_service", "devices"'
    function = _service_function(main_cpp, "legacy_service", NONE, "StringRef")
    assert "Automation<StringRef>" not in main_cpp
    assert '"LEGACY %s", devices.c_str()' in _function_body(main_cpp, function)


def test_cleanup_action_id_avoids_config_ids(generate_main) -> None:
    """The response cleanup action does not take an id the config already uses."""
    main_cpp = generate_main(CONFIG)
    last = _actions_of(main_cpp, "worker")[-1]
    assert last.startswith("worker__after")
    assert last != "worker__after"
    assert _action_code(main_cpp, last) == f"\n  {UNREGISTER}"
