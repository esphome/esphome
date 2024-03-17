"""Tests for the wizard.py file."""

import os

import esphome.wizard as wz
import pytest
from esphome.core import CORE
from esphome.components.esp8266.boards import ESP8266_BOARD_PINS
from esphome.components.esp32.boards import ESP32_BOARD_PINS
from esphome.components.bk72xx.boards import BK72XX_BOARD_PINS
from esphome.components.rtl87xx.boards import RTL87XX_BOARD_PINS
from unittest.mock import MagicMock


@pytest.fixture
def default_config():
    return {
        "name": "test-name",
        "platform": "ESP8266",
        "board": "esp01_1m",
        "ssid": "test_ssid",
        "psk": "test_psk",
        "password": "",
    }


@pytest.fixture
def wizard_answers():
    return [
        "test-node",  # Name of the node
        "ESP8266",  # platform
        "nodemcuv2",  # board
        "SSID",  # ssid
        "psk",  # wifi password
        "ota_pass",  # ota password
    ]


def test_sanitize_quotes_replaces_with_escaped_char():
    """
    The sanitize_quotes function should replace double quotes with their escaped equivalents
    """
    # Given
    input_str = '"key": "value"'

    # When
    output_str = wz.sanitize_double_quotes(input_str)

    # Then
    assert output_str == '\\"key\\": \\"value\\"'


def test_config_file_fallback_ap_includes_descriptive_name(default_config):
    """
    The fallback AP should include the node and a descriptive name
    """
    # Given
    default_config["name"] = "test_node"

    # When
    config = wz.wizard_file(**default_config)

    # Then
    assert 'ssid: "Test Node Fallback Hotspot"' in config


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
    assert 'ssid: "A Very Long Name For This Node"' in config


def test_config_file_should_include_ota(default_config):
    """
    The Over-The-Air update should be enabled by default
    """
    # Given

    # When
    config = wz.wizard_file(**default_config)

    # Then
    assert "ota:" in config


def test_config_file_should_include_ota_when_password_set(default_config):
    """
    The Over-The-Air update should be enabled when a password is set
    """
    # Given
    default_config["password"] = "foo"

    # When
    config = wz.wizard_file(**default_config)

    # Then
    assert "ota:" in config


def test_wizard_write_sets_platform(default_config, tmp_path, monkeypatch):
    """
    If the platform is not explicitly set, use "ESP8266" if the board is one of the ESP8266 boards
    """
    # Given
    del default_config["platform"]
    monkeypatch.setattr(wz, "write_file", MagicMock())
    monkeypatch.setattr(CORE, "config_path", os.path.dirname(tmp_path))

    # When
    wz.wizard_write(tmp_path, **default_config)

    # Then
    generated_config = wz.write_file.call_args.args[1]
    assert "esp8266:" in generated_config


def test_wizard_write_defaults_platform_from_board_esp8266(
    default_config, tmp_path, monkeypatch
):
    """
    If the platform is not explicitly set, use "ESP8266" if the board is one of the ESP8266 boards
    """
    # Given
    del default_config["platform"]
    default_config["board"] = [*ESP8266_BOARD_PINS][0]

    monkeypatch.setattr(wz, "write_file", MagicMock())
    monkeypatch.setattr(CORE, "config_path", os.path.dirname(tmp_path))

    # When
    wz.wizard_write(tmp_path, **default_config)

    # Then
    generated_config = wz.write_file.call_args.args[1]
    assert "esp8266:" in generated_config


def test_wizard_write_defaults_platform_from_board_esp32(
    default_config, tmp_path, monkeypatch
):
    """
    If the platform is not explicitly set, use "ESP32" if the board is one of the ESP32 boards
    """
    # Given
    del default_config["platform"]
    default_config["board"] = [*ESP32_BOARD_PINS][0]

    monkeypatch.setattr(wz, "write_file", MagicMock())
    monkeypatch.setattr(CORE, "config_path", os.path.dirname(tmp_path))

    # When
    wz.wizard_write(tmp_path, **default_config)

    # Then
    generated_config = wz.write_file.call_args.args[1]
    assert "esp32:" in generated_config


def test_wizard_write_defaults_platform_from_board_bk72xx(
    default_config, tmp_path, monkeypatch
):
    """
    If the platform is not explicitly set, use "BK72XX" if the board is one of BK72XX boards
    """
    # Given
    del default_config["platform"]
    default_config["board"] = [*BK72XX_BOARD_PINS][0]

    monkeypatch.setattr(wz, "write_file", MagicMock())
    monkeypatch.setattr(CORE, "config_path", os.path.dirname(tmp_path))

    # When
    wz.wizard_write(tmp_path, **default_config)

    # Then
    generated_config = wz.write_file.call_args.args[1]
    assert "bk72xx:" in generated_config


def test_wizard_write_defaults_platform_from_board_rtl87xx(
    default_config, tmp_path, monkeypatch
):
    """
    If the platform is not explicitly set, use "RTL87XX" if the board is one of RTL87XX boards
    """
    # Given
    del default_config["platform"]
    default_config["board"] = [*RTL87XX_BOARD_PINS][0]

    monkeypatch.setattr(wz, "write_file", MagicMock())
    monkeypatch.setattr(CORE, "config_path", os.path.dirname(tmp_path))

    # When
    wz.wizard_write(tmp_path, **default_config)

    # Then
    generated_config = wz.write_file.call_args.args[1]
    assert "rtl87xx:" in generated_config


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
    all_args = [
        call.args[0] if len(call.args) else "" for call in wz.safe_print.call_args_list
    ]

    assert any(step_desc == arg for arg in all_args)
    assert any(f"STEP {step_num}" in arg for arg in all_args)


def test_default_input_uses_default_if_no_input_supplied(monkeypatch):
    """
    The default_input() function should return the supplied default value if the user doesn't enter anything
    """

    # Given
    monkeypatch.setattr("builtins.input", lambda _=None: "")
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
    monkeypatch.setattr("builtins.input", lambda _=None: user_input)
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
    input_str = "Kühne"
    expected_str = "Kuhne"

    # When
    output_str = wz.strip_accents(input_str)

    # Then
    assert output_str == expected_str


def test_wizard_rejects_path_with_invalid_extension():
    """
    The wizard should reject config files that are not yaml
    """

    # Given
    config_file = "test.json"

    # When
    retval = wz.wizard(config_file)

    # Then
    assert retval == 1


def test_wizard_rejects_existing_files(tmpdir):
    """
    The wizard should reject any configuration file that already exists
    """

    # Given
    config_file = tmpdir.join("test.yaml")
    config_file.write("")

    # When
    retval = wz.wizard(str(config_file))

    # Then
    assert retval == 2


def test_wizard_accepts_default_answers_esp8266(tmpdir, monkeypatch, wizard_answers):
    """
    The wizard should accept the given default answers for esp8266
    """

    # Given
    config_file = tmpdir.join("test.yaml")
    input_mock = MagicMock(side_effect=wizard_answers)
    monkeypatch.setattr("builtins.input", input_mock)
    monkeypatch.setattr(wz, "safe_print", lambda t=None, end=None: 0)
    monkeypatch.setattr(wz, "sleep", lambda _: 0)
    monkeypatch.setattr(wz, "wizard_write", MagicMock())

    # When
    retval = wz.wizard(str(config_file))

    # Then
    assert retval == 0


def test_wizard_accepts_default_answers_esp32(tmpdir, monkeypatch, wizard_answers):
    """
    The wizard should accept the given default answers for esp32
    """

    # Given
    wizard_answers[1] = "ESP32"
    wizard_answers[2] = "nodemcu-32s"
    config_file = tmpdir.join("test.yaml")
    input_mock = MagicMock(side_effect=wizard_answers)
    monkeypatch.setattr("builtins.input", input_mock)
    monkeypatch.setattr(wz, "safe_print", lambda t=None, end=None: 0)
    monkeypatch.setattr(wz, "sleep", lambda _: 0)
    monkeypatch.setattr(wz, "wizard_write", MagicMock())

    # When
    retval = wz.wizard(str(config_file))

    # Then
    assert retval == 0


def test_wizard_offers_better_node_name(tmpdir, monkeypatch, wizard_answers):
    """
    When the node name does not conform, a better alternative is offered
    * Removes special chars
    * Replaces spaces with hyphens
    * Replaces underscores with hyphens
    * Converts all uppercase letters to lowercase
    """

    # Given
    wizard_answers[0] = "Küche_Unten #2"
    expected_name = "kuche-unten-2"
    monkeypatch.setattr(
        wz, "default_input", MagicMock(side_effect=lambda _, default: default)
    )

    config_file = tmpdir.join("test.yaml")
    input_mock = MagicMock(side_effect=wizard_answers)
    monkeypatch.setattr("builtins.input", input_mock)
    monkeypatch.setattr(wz, "safe_print", lambda t=None, end=None: 0)
    monkeypatch.setattr(wz, "sleep", lambda _: 0)
    monkeypatch.setattr(wz, "wizard_write", MagicMock())

    # When
    retval = wz.wizard(str(config_file))

    # Then
    assert retval == 0
    assert wz.default_input.call_args.args[1] == expected_name


def test_wizard_requires_correct_platform(tmpdir, monkeypatch, wizard_answers):
    """
    When the platform is not either esp32 or esp8266, the wizard should reject it
    """

    # Given
    wizard_answers.insert(1, "foobar")  # add invalid entry for platform

    config_file = tmpdir.join("test.yaml")
    input_mock = MagicMock(side_effect=wizard_answers)
    monkeypatch.setattr("builtins.input", input_mock)
    monkeypatch.setattr(wz, "safe_print", lambda t=None, end=None: 0)
    monkeypatch.setattr(wz, "sleep", lambda _: 0)
    monkeypatch.setattr(wz, "wizard_write", MagicMock())

    # When
    retval = wz.wizard(str(config_file))

    # Then
    assert retval == 0


def test_wizard_requires_correct_board(tmpdir, monkeypatch, wizard_answers):
    """
    When the board is not a valid esp8266 board, the wizard should reject it
    """

    # Given
    wizard_answers.insert(2, "foobar")  # add an invalid entry for board

    config_file = tmpdir.join("test.yaml")
    input_mock = MagicMock(side_effect=wizard_answers)
    monkeypatch.setattr("builtins.input", input_mock)
    monkeypatch.setattr(wz, "safe_print", lambda t=None, end=None: 0)
    monkeypatch.setattr(wz, "sleep", lambda _: 0)
    monkeypatch.setattr(wz, "wizard_write", MagicMock())

    # When
    retval = wz.wizard(str(config_file))

    # Then
    assert retval == 0


def test_wizard_requires_valid_ssid(tmpdir, monkeypatch, wizard_answers):
    """
    When the board is not a valid esp8266 board, the wizard should reject it
    """

    # Given
    wizard_answers.insert(3, "")  # add an invalid entry for ssid

    config_file = tmpdir.join("test.yaml")
    input_mock = MagicMock(side_effect=wizard_answers)
    monkeypatch.setattr("builtins.input", input_mock)
    monkeypatch.setattr(wz, "safe_print", lambda t=None, end=None: 0)
    monkeypatch.setattr(wz, "sleep", lambda _: 0)
    monkeypatch.setattr(wz, "wizard_write", MagicMock())

    # When
    retval = wz.wizard(str(config_file))

    # Then
    assert retval == 0
