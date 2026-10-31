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


def test_no_args_service(generate_main) -> None:
    """A service without arguments triggers an Automation of its merged inline actions."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "api_userservicetrigger_id") == (
        "api::UserServiceTrigger<api::enums::SUPPORTS_RESPONSE_NONE>"
        "(api_action0_strings, 4067713328UL)"
    )
    assert _constructed(main_cpp, "automation_id") == (
        "Automation<>(api_userservicetrigger_id)"
    )
    assert _actions_of(main_cpp, "automation_id") == ["lambdaaction_id"]
    assert _constructed(main_cpp, "lambdaaction_id") == (
        "ApplyRunAction<esphome__lambdaaction_id__fn>()"
    )
    body = _function_body(main_cpp, "esphome__lambdaaction_id__fn")
    assert "NO_ARGS called" in body
    assert "::doubled->value() = 1;" in body


def test_mixed_args_types(generate_main) -> None:
    """Strings and arrays are non-owning views when every action is synchronous."""
    main_cpp = generate_main(CONFIG)
    types = (
        "bool, int32_t, float, StringRef, const FixedVector<int32_t> &, "
        "const FixedVector<std::string> &"
    )
    assert _constructed(main_cpp, "api_userservicetrigger_id_2") == (
        f"api::UserServiceTrigger<api::enums::SUPPORTS_RESPONSE_NONE, {types}>"
        "(api_action1_strings, 2898783538UL)"
    )
    assert _constructed(main_cpp, "automation_id_2") == (
        f"Automation<{types}>(api_userservicetrigger_id_2)"
    )
    assert (
        'api_action1_strings[] PROGMEM = {"mixed_args", "bool_", "int_", "float_", '
        '"string_", "int_arr", "string_arr"};' in main_cpp
    )
    assert _actions_of(main_cpp, "automation_id_2") == ["lambdaaction_id_2"]
    assert "string_.c_str(), int_arr.size(), string_arr.size()" in main_cpp


def test_lambda_action_gets_args_by_value(generate_main) -> None:
    """A lambda action takes the arguments by value, so ``x *= 2`` compiles in both forms."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "api_userservicetrigger_id_3") == (
        "api::UserServiceTrigger<api::enums::SUPPORTS_RESPONSE_NONE, int32_t>"
        "(api_action2_strings, 3911810908UL)"
    )
    assert _actions_of(main_cpp, "automation_id_3") == ["lambdaaction_id_3"]
    body = _function_body(main_cpp, "esphome__lambdaaction_id_3__fn")
    assert body.count("[](int32_t x) -> void {") == 2
    assert "x *= 2;" in body
    assert '"BY_VALUE again %d", x *= 2' in body


def test_delay_uses_owning_args(generate_main) -> None:
    """A delay keeps separate actions and owning string and array arguments."""
    main_cpp = generate_main(CONFIG)
    types = "std::string, std::vector<int32_t>"
    assert _constructed(main_cpp, "api_userservicetrigger_id_4") == (
        f"api::UserServiceTrigger<api::enums::SUPPORTS_RESPONSE_NONE, {types}>"
        "(api_action3_strings, 1486811133)"
    )
    assert _constructed(main_cpp, "automation_id_4") == (
        f"Automation<{types}>(api_userservicetrigger_id_4)"
    )
    assert _actions_of(main_cpp, "automation_id_4") == [
        "lambdaaction_id_5",
        "delayaction_id",
        "lambdaaction_id_6",
    ]


def test_respond_optional(generate_main) -> None:
    """An optional response passes call_id and return_response; the call is unregistered last."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "api_userservicetrigger_id_5") == (
        "api::UserServiceTrigger<api::enums::SUPPORTS_RESPONSE_OPTIONAL, int32_t>"
        "(api_action4_strings, 89086895)"
    )
    assert _constructed(main_cpp, "automation_id_5") == (
        "Automation<uint32_t, bool, int32_t>(api_userservicetrigger_id_5)"
    )
    assert _constructed(main_cpp, "api_apirespondaction_id") == (
        "api::APIRespondAction<uint32_t, bool, int32_t>(api_apiserver_id)"
    )
    assert "api_apirespondaction_id->set_is_optional_mode(true);" in main_cpp
    assert _actions_of(main_cpp, "automation_id_5") == [
        "api_apirespondaction_id",
        "api_userservicetrigger_id_5__unregister",
    ]
    assert _constructed(main_cpp, "api_userservicetrigger_id_5__unregister") == (
        "api::APIUnregisterServiceCallAction<uint32_t, bool, int32_t>(api_apiserver_id)"
    )


def test_respond_only(generate_main) -> None:
    """A response-only service passes call_id; the call is unregistered after every action."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "api_userservicetrigger_id_6") == (
        "api::UserServiceTrigger<api::enums::SUPPORTS_RESPONSE_ONLY, StringRef>"
        "(api_action5_strings, 2886062137UL)"
    )
    assert _constructed(main_cpp, "automation_id_6") == (
        "Automation<uint32_t, StringRef>(api_userservicetrigger_id_6)"
    )
    assert "api_apirespondaction_id_2->set_is_optional_mode" not in main_cpp
    assert _actions_of(main_cpp, "automation_id_6") == [
        "lambdaaction_id_7",
        "api_apirespondaction_id_2",
        "api_userservicetrigger_id_6__unregister",
    ]
    assert _constructed(main_cpp, "api_userservicetrigger_id_6__unregister") == (
        "api::APIUnregisterServiceCallAction<uint32_t, StringRef>(api_apiserver_id)"
    )


def test_manual_trigger_id(generate_main) -> None:
    """A trigger_id set by hand names the service object."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "manual_service_trigger") == (
        "api::UserServiceTrigger<api::enums::SUPPORTS_RESPONSE_NONE>"
        "(api_action6_strings, 3319738294UL)"
    )
    assert _constructed(main_cpp, "automation_id_7") == (
        "Automation<>(manual_service_trigger)"
    )


def test_services_registered_in_order(generate_main) -> None:
    """Every service is registered with the server once, in config order."""
    main_cpp = generate_main(CONFIG)
    assert (
        "api_apiserver_id->initialize_user_services({api_userservicetrigger_id, "
        "api_userservicetrigger_id_2, api_userservicetrigger_id_3, "
        "api_userservicetrigger_id_4, api_userservicetrigger_id_5, "
        "api_userservicetrigger_id_6, manual_service_trigger});" in main_cpp
    )


def test_legacy_services_key(generate_main) -> None:
    """The legacy ``services:`` key with ``service:`` names builds the same service."""
    main_cpp = generate_main(LEGACY_CONFIG)
    assert 'api_action0_strings[] PROGMEM = {"legacy_service", "devices"};' in main_cpp
    assert _constructed(main_cpp, "api_userservicetrigger_id") == (
        "api::UserServiceTrigger<api::enums::SUPPORTS_RESPONSE_NONE, StringRef>"
        "(api_action0_strings, 2510686252UL)"
    )
    assert _constructed(main_cpp, "automation_id") == (
        "Automation<StringRef>(api_userservicetrigger_id)"
    )
    assert _actions_of(main_cpp, "automation_id") == ["lambdaaction_id"]
    assert '"LEGACY %s", devices.c_str()' in main_cpp
