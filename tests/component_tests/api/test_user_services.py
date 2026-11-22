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
from tests.component_tests.helpers import constructed, function_body

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


def _actions_of(main_cpp: str, automation: str) -> list[str]:
    return re.findall(rf"\b{automation}->add_action\((\w+)\);", main_cpp)


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
        function_body(main_cpp, function),
    )
    assert match is not None, function
    return match.group(1)


def _action_code(main_cpp: str, action: str) -> str:
    """Return the code an action runs: its generated function, or its constructor."""
    code = constructed(main_cpp, action)
    if match := re.match(r"Apply(?:Run)?Action<(\w+)[,>]", code):
        return function_body(main_cpp, match.group(1))
    return code


def _action_class(main_cpp: str, action: str) -> str:
    return re.match(r"[\w:]+", constructed(main_cpp, action)).group(0)


def test_no_args_service(generate_main) -> None:
    """Inline actions run in one function the service calls, with no Automation."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "no_args", NONE)
    body = function_body(main_cpp, function)
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
    body = function_body(main_cpp, function)
    assert "string_.c_str(), int_arr.size(), string_arr.size()" in body


def test_lambda_action_gets_args_by_value(generate_main) -> None:
    """A lambda action takes the arguments by value, so ``x *= 2`` compiles in both forms."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "by_value", NONE, "int32_t")
    body = function_body(main_cpp, function)
    assert body.count("[](int32_t x) -> void {") == 2
    assert "x *= 2;" in body
    assert '"BY_VALUE again %d", x *= 2' in body


def test_delay_uses_owning_args(generate_main) -> None:
    """A delay keeps an Automation of separate actions, which the function triggers."""
    main_cpp = generate_main(CONFIG)
    types = "std::string, std::vector<int32_t>"
    function = _service_function(main_cpp, "with_delay", NONE, types)
    automation = _triggered_automation(main_cpp, function, "name, int_arr")
    assert constructed(main_cpp, automation) == f"Automation<{types}>()"
    before, delay, after = _actions_of(main_cpp, automation)
    assert _action_class(main_cpp, delay) == "DelayAction"
    assert "WITH_DELAY before" in _action_code(main_cpp, before)
    after_code = _action_code(main_cpp, after)
    assert "WITH_DELAY %s %zu" in after_code
    assert "[](std::string name, std::vector<int32_t> int_arr) -> void" in after_code


SEND_JSON = (
    "::api_apiserver_id->send_action_response_json(call_id, true, "
    '::esphome::StringRef::from_lit(""), '
)


def test_respond_optional(generate_main) -> None:
    """An optional response passes return_response, so the data goes only to a client that asked."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "respond_optional", OPTIONAL, "int32_t")
    body = function_body(main_cpp, function)
    assert "->trigger(" not in body
    assert body.index(SEND_JSON) < body.index('root["value"] = value;')
    assert "}(call_id, return_response, value, root); }, return_response);" in body
    assert body.endswith(f"\n  {{\n  {UNREGISTER}\n  }}")


def test_respond_only(generate_main) -> None:
    """A response-only service always sends its data, after the actions before it, then unregisters."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "respond_only", ONLY, "StringRef")
    body = function_body(main_cpp, function)
    assert "->trigger(" not in body
    assert body.index("RESPOND_ONLY %s") < body.index(SEND_JSON)
    assert "}(call_id, name, root); });" in body
    assert body.index('root["name"] = name;') < body.index(UNREGISTER)


@pytest.mark.parametrize(
    ("name", "types", "call"),
    [
        (
            "respond_only_named_return_response",
            "int32_t",
            "}(call_id, return_response, root); });",
        ),
        (
            "respond_only_bool_named_return_response",
            "int32_t, bool",
            "}(call_id, value, return_response, root); });",
        ),
    ],
)
def test_respond_only_variable_named_return_response(
    generate_main, name: str, types: str, call: str
) -> None:
    """A response-only variable named return_response does not make the data optional."""
    main_cpp = generate_main(CONFIG)
    body = function_body(main_cpp, _service_function(main_cpp, name, ONLY, types))
    assert call in body


def test_respond_optional_inside_repeat(generate_main) -> None:
    """Inside repeat:, return_response is not the second argument and still makes the data optional."""
    main_cpp = generate_main(CONFIG)
    assert (
        "}(iteration, call_id, return_response, root); }, return_response);" in main_cpp
    )


def test_respond_data_copies_named_args(generate_main) -> None:
    """The data lambda gets its own copy of an arg it names, and none of one it ignores."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(
        main_cpp, "respond_data_copies_named_args", ONLY, "StringRef, int32_t"
    )
    body = function_body(main_cpp, function)
    assert "const std::remove_cvref_t<StringRef> & big, int32_t count, " in body
    assert "int32_t & count" not in body


def test_respond_data_bare_return(generate_main) -> None:
    """A data lambda whose only statement is `return;` is called, not reduced to a cast."""
    main_cpp = generate_main(CONFIG)
    body = function_body(
        main_cpp, _service_function(main_cpp, "respond_data_bare_return", ONLY)
    )
    assert "send_action_response_json(" in body
    assert "static_cast<void>()" not in body


def test_respond_status_lambdas(generate_main) -> None:
    """Lambdas for the status and the error text are bound before the response is sent."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "respond_error_lambda", STATUS)
    body = function_body(main_cpp, function)
    send = (
        "::api_apiserver_id->send_action_response(call_id, esphome__success, "
        "::esphome::StringRef(esphome__error_message));"
    )
    assert body.index("const bool esphome__success = ") < body.index(send)
    assert body.index("const std::string esphome__error_message = ") < body.index(send)


def test_respond_constant_error(generate_main) -> None:
    """A constant error text is a literal, with no string copy."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "respond_error_constant", STATUS)
    body = function_body(main_cpp, function)
    assert (
        '::api_apiserver_id->send_action_response(call_id, false, ::esphome::StringRef::from_lit("not ready"));'
        in body
    )


def test_unregister_merges_with_trailing_inline_actions(generate_main) -> None:
    """After a non-inline action, the unregister joins the inline actions that follow it."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "respond_then_log", STATUS)
    automation = _triggered_automation(main_cpp, function, "call_id")
    respond, delay, log = _actions_of(main_cpp, automation)
    assert (
        'send_action_response(call_id, true, ::esphome::StringRef::from_lit(""));'
        in _action_code(main_cpp, respond)
    )
    assert _action_class(main_cpp, delay) == "DelayAction"
    body = _action_code(main_cpp, log)
    assert body.index("RESPONDED") < body.index("if (!esphome__running)")
    assert body.endswith(f"{UNREGISTER}\n  }}")


def test_unregister_in_fully_inline_response_service(generate_main) -> None:
    """With only inline actions, the unregister ends the function the service calls."""
    main_cpp = generate_main(CONFIG)
    function = _service_function(main_cpp, "status_inline", STATUS)
    body = function_body(main_cpp, function)
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
    assert "MANUAL called" in function_body(main_cpp, function)


def test_lambda_uses_own_service_id(generate_main) -> None:
    """A lambda in a service can refer to the service's own id."""
    main_cpp = generate_main(CONFIG)
    service = _service(main_cpp, "self_reference")
    assert service.id == "self_reference_service"
    function = _service_function(main_cpp, "self_reference", NONE)
    body = function_body(main_cpp, function)
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
        "respond_error_lambda",
        "respond_only_named_return_response",
        "respond_only_bool_named_return_response",
        "respond_optional_in_repeat",
        "respond_data_copies_named_args",
        "respond_data_bare_return",
        "respond_error_constant",
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
    assert '"LEGACY %s", devices.c_str()' in function_body(main_cpp, function)


def test_cleanup_action_id_avoids_config_ids(generate_main) -> None:
    """The response cleanup action does not take an id the config already uses."""
    main_cpp = generate_main(CONFIG)
    last = _actions_of(main_cpp, "worker")[-1]
    assert last.startswith("worker__after")
    assert last != "worker__after"
    assert _action_code(main_cpp, last) == f"\n  {UNREGISTER}"
