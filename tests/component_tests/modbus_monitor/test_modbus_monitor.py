"""Tests for the modbus_monitor component."""

import pytest

from esphome import config_validation as cv
from esphome.components import modbus_monitor
from tests.component_tests.helpers import get_define_value

DIR = "tests/component_tests/modbus_monitor"


def test_callbacks_are_added_to_their_hubs(generate_main) -> None:
    main_cpp = generate_main(f"{DIR}/test_modbus_monitor.yaml")
    assert main_cpp.count("server_hub->add_on_request_callback(") == 2
    assert main_cpp.count("client_hub->add_on_request_callback(") == 1
    assert "plain_hub->add_on_request_callback(" not in main_cpp
    # Sized for the hub with the most blocks.
    assert get_define_value("MODBUS_ON_REQUEST_COUNT") == "2"
    # The client hub's block has only on_response.
    assert main_cpp.count("client_hub->add_on_response_callback(") == 1
    assert "server_hub->add_on_response_callback(" not in main_cpp
    assert get_define_value("MODBUS_ON_RESPONSE_COUNT") == "1"
    assert "server_hub->send_response(" in main_cpp
    assert get_define_value("USE_MODBUS_SEND_RESPONSE") is not None


def test_hub_is_found_without_modbus_id(generate_main) -> None:
    main_cpp = generate_main(f"{DIR}/test_default_id.yaml")
    assert "only_hub->add_on_request_callback(" in main_cpp
    assert get_define_value("MODBUS_ON_REQUEST_COUNT") == "1"


def test_nothing_is_compiled_in_without_a_block(generate_main) -> None:
    main_cpp = generate_main(f"{DIR}/test_no_monitor.yaml")
    assert "add_on_request_callback(" not in main_cpp
    assert "add_on_response_callback(" not in main_cpp
    assert get_define_value("MODBUS_ON_REQUEST_COUNT") is None
    assert get_define_value("MODBUS_ON_RESPONSE_COUNT") is None
    assert get_define_value("USE_MODBUS_SEND_RESPONSE") is None


def test_empty_block_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="on_request"):
        modbus_monitor.CONFIG_SCHEMA({"modbus_id": "hub"})
