"""Code generation of api user-defined actions (``api: actions:`` and legacy ``services:``)."""

import re

CONFIG = "tests/component_tests/api/test_user_services.yaml"
LEGACY_CONFIG = "tests/component_tests/api/test_user_services_legacy.yaml"


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


NONE = "api::enums::SUPPORTS_RESPONSE_NONE"
OPTIONAL = "api::enums::SUPPORTS_RESPONSE_OPTIONAL"
ONLY = "api::enums::SUPPORTS_RESPONSE_ONLY"
UNREGISTER_BODY = (
    "\n  if (call_id != 0)\n"
    "    ::api_apiserver_id->unregister_active_action_call(call_id);"
)


def test_no_args_service(generate_main) -> None:
    """Inline actions run in one function the service calls, with no Automation."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "api_userservicecallback_id") == (
        f"api::UserServiceCallback<esphome__lambdaaction_id__fn, {NONE}>"
        "(api_action0_strings, 4067713328UL)"
    )
    body = _function_body(main_cpp, "esphome__lambdaaction_id__fn")
    assert "NO_ARGS called" in body
    assert "::doubled->value() = 1;" in body
    assert "esphome__running" not in body
    assert "new(lambdaaction_id)" not in main_cpp
    assert "Automation<>" not in main_cpp


def test_mixed_args_types(generate_main) -> None:
    """Strings and arrays are non-owning views when every action is synchronous."""
    main_cpp = generate_main(CONFIG)
    types = (
        "bool, int32_t, float, StringRef, const FixedVector<int32_t> &, "
        "const FixedVector<std::string> &"
    )
    assert _constructed(main_cpp, "api_userservicecallback_id_2") == (
        f"api::UserServiceCallback<esphome__lambdaaction_id_2__fn, {NONE}, {types}>"
        "(api_action1_strings, 2898783538UL)"
    )
    assert (
        'api_action1_strings[] PROGMEM = {"mixed_args", "bool_", "int_", "float_", '
        '"string_", "int_arr", "string_arr"};' in main_cpp
    )
    # Arrays stay non-owning references in the generated function
    assert (
        "const FixedVector<int32_t> & int_arr, "
        "const FixedVector<std::string> & string_arr) {" in main_cpp
    )
    body = _function_body(main_cpp, "esphome__lambdaaction_id_2__fn")
    assert "string_.c_str(), int_arr.size(), string_arr.size()" in body


def test_lambda_action_gets_args_by_value(generate_main) -> None:
    """A lambda action takes the arguments by value, so ``x *= 2`` compiles in both forms."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "api_userservicecallback_id_3") == (
        f"api::UserServiceCallback<esphome__lambdaaction_id_3__fn, {NONE}, int32_t>"
        "(api_action2_strings, 3911810908UL)"
    )
    body = _function_body(main_cpp, "esphome__lambdaaction_id_3__fn")
    assert body.count("[](int32_t x) -> void {") == 2
    assert "x *= 2;" in body
    assert '"BY_VALUE again %d", x *= 2' in body


def test_delay_uses_owning_args(generate_main) -> None:
    """A delay keeps an Automation of separate actions, which the function triggers."""
    main_cpp = generate_main(CONFIG)
    types = "std::string, std::vector<int32_t>"
    assert _constructed(main_cpp, "api_userservicecallback_id_4") == (
        f"api::UserServiceCallback<esphome__automation_id_4__fn, {NONE}, {types}>"
        "(api_action3_strings, 1486811133)"
    )
    assert _constructed(main_cpp, "automation_id_4") == f"Automation<{types}>()"
    assert _function_body(main_cpp, "esphome__automation_id_4__fn") == (
        "\n  ::automation_id_4->trigger(name, int_arr);"
    )
    assert _actions_of(main_cpp, "automation_id_4") == [
        "lambdaaction_id_5",
        "delayaction_id",
        "lambdaaction_id_6",
    ]


def test_respond_optional(generate_main) -> None:
    """An optional response passes call_id and return_response; the call is unregistered last."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "api_userservicecallback_id_5") == (
        f"api::UserServiceCallback<esphome__automation_id_5__fn, {OPTIONAL}, int32_t>"
        "(api_action4_strings, 89086895)"
    )
    assert _constructed(main_cpp, "automation_id_5") == (
        "Automation<uint32_t, bool, int32_t>()"
    )
    assert _function_body(main_cpp, "esphome__automation_id_5__fn") == (
        "\n  ::automation_id_5->trigger(call_id, return_response, value);"
    )
    assert _constructed(main_cpp, "api_apirespondaction_id") == (
        "api::APIRespondAction<uint32_t, bool, int32_t>(api_apiserver_id)"
    )
    assert "api_apirespondaction_id->set_is_optional_mode(true);" in main_cpp
    assert _actions_of(main_cpp, "automation_id_5") == [
        "api_apirespondaction_id",
        "automation_id_5__after",
    ]
    assert _constructed(main_cpp, "automation_id_5__after") == (
        "ApplyAction<esphome__automation_id_5__after__fn, uint32_t, bool, int32_t>()"
    )
    assert (
        _function_body(main_cpp, "esphome__automation_id_5__after__fn")
        == UNREGISTER_BODY
    )


def test_respond_only(generate_main) -> None:
    """A response-only service passes call_id; the call is unregistered after every action."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "api_userservicecallback_id_6") == (
        f"api::UserServiceCallback<esphome__automation_id_6__fn, {ONLY}, StringRef>"
        "(api_action5_strings, 2886062137UL)"
    )
    assert _constructed(main_cpp, "automation_id_6") == (
        "Automation<uint32_t, StringRef>()"
    )
    assert _function_body(main_cpp, "esphome__automation_id_6__fn") == (
        "\n  ::automation_id_6->trigger(call_id, name);"
    )
    assert "api_apirespondaction_id_2->set_is_optional_mode" not in main_cpp
    assert _actions_of(main_cpp, "automation_id_6") == [
        "lambdaaction_id_7",
        "api_apirespondaction_id_2",
        "automation_id_6__after",
    ]
    assert _constructed(main_cpp, "automation_id_6__after") == (
        "ApplyAction<esphome__automation_id_6__after__fn, uint32_t, StringRef>()"
    )
    assert (
        _function_body(main_cpp, "esphome__automation_id_6__after__fn")
        == UNREGISTER_BODY
    )


def test_unregister_merges_with_trailing_inline_actions(generate_main) -> None:
    """After a non-inline action, the unregister joins the inline actions that follow it."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "api_userservicecallback_id_7") == (
        "api::UserServiceCallback<esphome__automation_id_8__fn, "
        "api::enums::SUPPORTS_RESPONSE_STATUS>(api_action7_strings, 738580677)"
    )
    assert _actions_of(main_cpp, "automation_id_8") == [
        "api_apirespondaction_id_3",
        "lambdaaction_id_9",
    ]
    body = _function_body(main_cpp, "esphome__lambdaaction_id_9__fn")
    assert body.index("RESPONDED") < body.index("if (!esphome__running)")
    assert body.endswith(
        "if (call_id != 0)\n"
        "    ::api_apiserver_id->unregister_active_action_call(call_id);\n  }"
    )


def test_manual_trigger_id(generate_main) -> None:
    """A trigger_id set by hand names the service object."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "manual_service_trigger") == (
        f"api::UserServiceCallback<esphome__lambdaaction_id_8__fn, {NONE}>"
        "(api_action6_strings, 3319738294UL)"
    )
    assert "MANUAL called" in _function_body(main_cpp, "esphome__lambdaaction_id_8__fn")


def test_services_registered_in_order(generate_main) -> None:
    """Every service is registered with the server once, in config order."""
    main_cpp = generate_main(CONFIG)
    assert (
        "api_apiserver_id->initialize_user_services({api_userservicecallback_id, "
        "api_userservicecallback_id_2, api_userservicecallback_id_3, "
        "api_userservicecallback_id_4, api_userservicecallback_id_5, "
        "api_userservicecallback_id_6, manual_service_trigger, "
        "api_userservicecallback_id_7});" in main_cpp
    )


def test_legacy_services_key(generate_main) -> None:
    """The legacy ``services:`` key with ``service:`` names builds the same service."""
    main_cpp = generate_main(LEGACY_CONFIG)
    assert 'api_action0_strings[] PROGMEM = {"legacy_service", "devices"};' in main_cpp
    assert _constructed(main_cpp, "api_userservicecallback_id") == (
        f"api::UserServiceCallback<esphome__lambdaaction_id__fn, {NONE}, StringRef>"
        "(api_action0_strings, 2510686252UL)"
    )
    assert "Automation<StringRef>" not in main_cpp
    assert '"LEGACY %s", devices.c_str()' in _function_body(
        main_cpp, "esphome__lambdaaction_id__fn"
    )
