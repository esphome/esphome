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


def test_empty_block_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="on_request"):
        modbus_monitor.CONFIG_SCHEMA({"modbus_id": "hub"})


def test_sensors_share_one_decoder_per_hub(generate_main) -> None:
    main_cpp = generate_main(f"{DIR}/test_sensor.yaml")
    # One decoder and one callback per hub, however many sensors name it.
    assert main_cpp.count("modbus_monitor::RegisterDecoder();") == 2
    assert (
        main_cpp.count("hub_a->add_on_response_callback(") == 2
    )  # decoder + the on_response block
    assert main_cpp.count("hub_b->add_on_response_callback(") == 1
    assert get_define_value("MODBUS_ON_RESPONSE_COUNT") == "2"
    # Each sensor joins the decoder of its own hub, and no other.
    for sensor_id in ("hub_a_register_100", "hub_a_register_101", "hub_a_register_102"):
        assert f"hub_a_register_decoder->add_item({sensor_id});" in main_cpp
    assert main_cpp.count("hub_a_register_decoder->add_item(") == 3
    assert "hub_b_register_decoder->add_item(hub_b_register_100);" in main_cpp
    assert main_cpp.count("hub_b_register_decoder->add_item(") == 1
    # value_type reaches the constructor: U_WORD by default, S_WORD as asked.
    assert (
        "new(hub_a_register_100) modbus_monitor::ModbusMonitorSensor"
        "(0x01, 100, modbus::helpers::SensorValueType::U_WORD);" in main_cpp
    )
    assert (
        "new(hub_a_register_101) modbus_monitor::ModbusMonitorSensor"
        "(0x01, 101, modbus::helpers::SensorValueType::S_WORD);" in main_cpp
    )
