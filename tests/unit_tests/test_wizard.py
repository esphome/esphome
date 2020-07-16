""" Tests for the wizard.py file """

import esphome.wizard as wz
import pytest
from esphome.pins import ESP8266_BOARD_PINS
from mock import MagicMock


@pytest.fixture
def default_config():
    return {
        "name": "test_name",
        "platform": "test_platform",
        "board": "test_board",
        "ssid": "test_ssid",
        "psk": "test_psk",
        "password": ""
    }


def test_sanitize_quotes_replaces_with_escaped_char():
    """
    The sanitize_quotes function should replace double quotes with their escaped equivalents
    """
    # Given
    input_str = "\"key\": \"value\""

    # When
    output_str = wz.sanitize_double_quotes(input_str)

    # Then
    assert output_str == "\\\"key\\\": \\\"value\\\""


def test_config_file_fallback_ap_includes_descriptive_name(default_config):
    """
    The fallback AP should include the node and a descriptive name
    """
    # Given

    # When
    config = wz.wizard_file(**default_config)

    # Then
    f"ssid: \"{default_config['name']} Fallback Hotspot\"" in config


def test_config_file_fallback_ap_name_less_than_32_chars(default_config):
    """
    The fallback AP name must be less than 32 chars.
    Since it is composed of the node name and "Fallback Hotspot" this can be too long and needs truncating
    """
    # Given
    default_config["name"] = "a_very_long_name_for_this_node"

    # When
    config = wz.wizard_file(**default_config)

    # Then
    f"ssid: \"{default_config['name']}\"" in config


def test_config_file_should_include_ota(default_config):
    """
    The Over-The-Air update should be enabled by default
    """
    # Given

    # When
    config = wz.wizard_file(**default_config)

    # Then
    "ota:" in config


def test_config_file_should_include_ota_when_password_set(default_config):
    """
    The Over-The-Air update should be enabled when a password is set
    """
    # Given
    default_config["password"] = "foo"

    # When
    config = wz.wizard_file(**default_config)

    # Then
    "ota:" in config


def test_wizard_write_sets_platform(default_config, tmp_path, monkeypatch):
    """
    If the platform is not explicitly set, use "ESP8266" if the board is one of the ESP8266 boards
    """
    # Given
    monkeypatch.setattr(wz, "write_file", MagicMock())

    # When
    wz.wizard_write(tmp_path, **default_config)

    # Then
    generated_config = wz.write_file.call_args.args[1]
    assert f"platform: {default_config['platform']}" in generated_config


def test_wizard_write_defaults_platform_from_board_esp8266(default_config, tmp_path, monkeypatch):
    """
    If the platform is not explicitly set, use "ESP8266" if the board is one of the ESP8266 boards
    """
    # Given
    del default_config["platform"]
    default_config["board"] = [*ESP8266_BOARD_PINS][0]

    monkeypatch.setattr(wz, "write_file", MagicMock())

    # When
    wz.wizard_write(tmp_path, **default_config)

    # Then
    generated_config = wz.write_file.call_args.args[1]
    assert "platform: ESP8266" in generated_config


def test_wizard_write_defaults_platform_from_board_esp32(default_config, tmp_path, monkeypatch):
    """
    If the platform is not explicitly set, use "ESP32" if the board is not one of the ESP8266 boards
    """
    # Given
    del default_config["platform"]
    default_config["board"] = "foo"

    monkeypatch.setattr(wz, "write_file", MagicMock())

    # When
    wz.wizard_write(tmp_path, **default_config)

    # Then
    generated_config = wz.write_file.call_args.args[1]
    assert "platform: ESP32" in generated_config


def test_safe_print_step_prints_step_number_and_description(monkeypatch):
    """
    The safe_print_step function prints the step number and the passed description
    """
    # Given
    monkeypatch.setattr(wz, "safe_print", MagicMock())
    monkeypatch.setattr(wz, "sleep", lambda time: 0)

    step_num = 22
    step_desc = "foobartest"

    # When
    wz.safe_print_step(step_num, step_desc)

    # Then
    # Collect arguments to all safe_print() calls (substituting "" for any empty ones)
    all_args = [call.args[0] if len(call.args) else "" for call in wz.safe_print.call_args_list]

    assert any(step_desc == arg for arg in all_args)
    assert any(f"STEP {step_num}" in arg for arg in all_args)


def test_default_input_uses_default_if_no_input_supplied(monkeypatch):
    """
    The default_input() function should return the supplied default value if the user doesn't enter anything
    """

    # Given
    monkeypatch.setattr("builtins.input", lambda _: "")
    default_string = "foobar"

    # When
    retval = wz.default_input("", default_string)

    # Then
    assert retval == default_string


def test_default_input_uses_user_supplied_value(monkeypatch):
    """
    The default_input() function should return the value that the user enters
    """

    # Given
    user_input = "A value"
    monkeypatch.setattr("builtins.input", lambda _: user_input)
    default_string = "foobar"

    # When
    retval = wz.default_input("", default_string)

    # Then
    assert retval == user_input


def test_strip_accents_removes_diacritics():
    """
    The strip_accents() function should remove diacritics (umlauts)
    """

    # Given
    input_str = u"Kühne"
    expected_str = "Kuhne"

    # When
    output_str = wz.strip_accents(input_str)

    # Then
    assert output_str == expected_str
