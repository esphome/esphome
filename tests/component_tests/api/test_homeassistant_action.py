"""Tests for arg-type selection of api user-defined services with homeassistant.action."""

CONFIG = "tests/component_tests/api/test_homeassistant_action.yaml"


def test_synchronous_chain_keeps_zero_copy_args(generate_main):
    """A chain of synchronous actions keeps the non-owning StringRef arg type."""
    main_cpp = generate_main(CONFIG)

    assert (
        "api::UserServiceTrigger<api::enums::SUPPORTS_RESPONSE_NONE, StringRef>"
        '("zero_copy_args", {"message"})' in main_cpp
    )


def test_response_callback_args_are_owning(generate_main):
    """homeassistant.action with on_success/on_error stores the trigger args
    until the HomeassistantActionResponse arrives, so string args must fall
    back to owning std::string; StringRef would point into the connection's
    receive buffer, which is reused before the response arrives."""
    main_cpp = generate_main(CONFIG)

    assert (
        "api::UserServiceTrigger<api::enums::SUPPORTS_RESPONSE_NONE, std::string>"
        '("response_args", {"message"})' in main_cpp
    )
    assert "api::HomeAssistantServiceCallAction<std::string>" in main_cpp
    assert "api::HomeAssistantServiceCallAction<StringRef>" not in main_cpp
