""" Tests for the wizard.py file """

import esphome.wizard as wz
import pytest


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
