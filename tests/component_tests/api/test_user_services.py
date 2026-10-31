"""Code generation of api user-defined actions (``api: actions:`` and legacy ``services:``).

Objects are found from the action name and the links between them, not from
auto-numbered ids, so adding or changing one action does not break the others.
"""

import re
from typing import NamedTuple

from esphome.helpers import fnv1_hash

CONFIG = "tests/component_tests/api/test_user_services.yaml"
LEGACY_CONFIG = "tests/component_tests/api/test_user_services_legacy.yaml"

NONE = "api::enums::SUPPORTS_RESPONSE_NONE"
OPTIONAL = "api::enums::SUPPORTS_RESPONSE_OPTIONAL"
ONLY = "api::enums::SUPPORTS_RESPONSE_ONLY"


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


def _automation(main_cpp: str, trigger: str) -> tuple[str, str]:
    """Return the id and template arguments of the Automation on ``trigger``."""
    match = re.search(rf"\bnew\((\w+)\) Automation<(.*?)>\({trigger}\);", main_cpp)
    assert match is not None, trigger
    return match.group(1), match.group(2)


def _action_code(main_cpp: str, action: str) -> str:
    """Return the code an action runs: its generated function, or its constructor."""
    constructed = _constructed(main_cpp, action)
    if match := re.match(r"ApplyRunAction<(\w+)[,>]", constructed):
        return _function_body(main_cpp, match.group(1))
    return constructed


def _action_class(main_cpp: str, action: str) -> str:
    return re.match(r"[\w:]+", _constructed(main_cpp, action)).group(0)


def test_no_args_service(generate_main) -> None:
    """A service without arguments triggers an Automation of its merged inline actions."""
    main_cpp = generate_main(CONFIG)
    service = _service(main_cpp, "no_args")
    assert service.type == f"api::UserServiceTrigger<{NONE}>"
    automation, args = _automation(main_cpp, service.id)
    assert args == ""
    (action,) = _actions_of(main_cpp, automation)
    assert _action_class(main_cpp, action) == "ApplyRunAction"
    code = _action_code(main_cpp, action)
    assert "NO_ARGS called" in code
    assert "::doubled->value() = 1;" in code


def test_mixed_args_types(generate_main) -> None:
    """Strings and arrays are non-owning views when every action is synchronous."""
    main_cpp = generate_main(CONFIG)
    types = (
        "bool, int32_t, float, StringRef, const FixedVector<int32_t> &, "
        "const FixedVector<std::string> &"
    )
    service = _service(main_cpp, "mixed_args")
    assert service.type == f"api::UserServiceTrigger<{NONE}, {types}>"
    automation, args = _automation(main_cpp, service.id)
    assert args == types
    assert _strings(main_cpp, "mixed_args")[1] == (
        '"mixed_args", "bool_", "int_", "float_", "string_", "int_arr", "string_arr"'
    )
    (action,) = _actions_of(main_cpp, automation)
    assert "string_.c_str(), int_arr.size(), string_arr.size()" in _action_code(
        main_cpp, action
    )


def test_lambda_action_gets_args_by_value(generate_main) -> None:
    """A lambda action takes the arguments by value, so ``x *= 2`` compiles in both forms."""
    main_cpp = generate_main(CONFIG)
    service = _service(main_cpp, "by_value")
    assert service.type == f"api::UserServiceTrigger<{NONE}, int32_t>"
    automation, _ = _automation(main_cpp, service.id)
    (action,) = _actions_of(main_cpp, automation)
    code = _action_code(main_cpp, action)
    assert code.count("[](int32_t x) -> void {") == 2
    assert "x *= 2;" in code
    assert '"BY_VALUE again %d", x *= 2' in code


def test_delay_uses_owning_args(generate_main) -> None:
    """A delay keeps separate actions and owning string and array arguments."""
    main_cpp = generate_main(CONFIG)
    types = "std::string, std::vector<int32_t>"
    service = _service(main_cpp, "with_delay")
    assert service.type == f"api::UserServiceTrigger<{NONE}, {types}>"
    automation, args = _automation(main_cpp, service.id)
    assert args == types
    before, delay, after = _actions_of(main_cpp, automation)
    assert _action_class(main_cpp, delay) == "DelayAction"
    assert "WITH_DELAY before" in _action_code(main_cpp, before)
    after_code = _action_code(main_cpp, after)
    assert "WITH_DELAY %s %zu" in after_code
    assert "[](std::string name, std::vector<int32_t> int_arr) -> void" in after_code


def test_respond_optional(generate_main) -> None:
    """An optional response passes call_id and return_response; the call is unregistered last."""
    main_cpp = generate_main(CONFIG)
    service = _service(main_cpp, "respond_optional")
    assert service.type == f"api::UserServiceTrigger<{OPTIONAL}, int32_t>"
    automation, args = _automation(main_cpp, service.id)
    assert args == "uint32_t, bool, int32_t"
    respond, unregister = _actions_of(main_cpp, automation)
    assert _constructed(main_cpp, respond) == (
        "api::APIRespondAction<uint32_t, bool, int32_t>(api_apiserver_id)"
    )
    assert f"{respond}->set_is_optional_mode(true);" in main_cpp
    assert unregister == f"{service.id}__unregister"
    assert _constructed(main_cpp, unregister) == (
        "api::APIUnregisterServiceCallAction<uint32_t, bool, int32_t>(api_apiserver_id)"
    )


def test_respond_only(generate_main) -> None:
    """A response-only service passes call_id; the call is unregistered after every action."""
    main_cpp = generate_main(CONFIG)
    service = _service(main_cpp, "respond_only")
    assert service.type == f"api::UserServiceTrigger<{ONLY}, StringRef>"
    automation, args = _automation(main_cpp, service.id)
    assert args == "uint32_t, StringRef"
    log, respond, unregister = _actions_of(main_cpp, automation)
    assert "RESPOND_ONLY %s" in _action_code(main_cpp, log)
    assert _constructed(main_cpp, respond) == (
        "api::APIRespondAction<uint32_t, StringRef>(api_apiserver_id)"
    )
    assert f"{respond}->set_is_optional_mode" not in main_cpp
    assert unregister == f"{service.id}__unregister"
    assert _constructed(main_cpp, unregister) == (
        "api::APIUnregisterServiceCallAction<uint32_t, StringRef>(api_apiserver_id)"
    )


def test_manual_trigger_id(generate_main) -> None:
    """A trigger_id set by hand names the service object."""
    main_cpp = generate_main(CONFIG)
    assert _service(main_cpp, "manual_trigger_id") == Service(
        "manual_service_trigger", f"api::UserServiceTrigger<{NONE}>"
    )
    automation, _ = _automation(main_cpp, "manual_service_trigger")
    (action,) = _actions_of(main_cpp, automation)
    assert "MANUAL called" in _action_code(main_cpp, action)


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
    service = _service(main_cpp, "legacy_service")
    assert service.type == f"api::UserServiceTrigger<{NONE}, StringRef>"
    automation, args = _automation(main_cpp, service.id)
    assert args == "StringRef"
    (action,) = _actions_of(main_cpp, automation)
    assert '"LEGACY %s", devices.c_str()' in _action_code(main_cpp, action)
