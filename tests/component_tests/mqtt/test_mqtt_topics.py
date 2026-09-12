"""Tests for MQTT topic generation with name_add_mac_suffix."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from esphome.components.mqtt import exp_mqtt_message


def test_exp_mqtt_message_none_config_returns_empty_optional() -> None:
    """A disabled message (config None) renders as an empty std::optional, not a struct."""
    result = exp_mqtt_message(None)

    assert str(result) == "std::optional<mqtt::MQTTMessage>()"


def test_mac_suffix_default_topics(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Default topics must be rebuilt from App.get_name() so they carry the MAC suffix."""
    main_cpp = generate_main(component_config_path("mac_suffix_default_topics.yaml"))

    assert "set_topic_prefix(App.get_name())" in main_cpp
    assert '(App.get_name() + "/status")' in main_cpp
    assert '(App.get_name() + "/debug")' in main_cpp

    assert '"test-device/status"' not in main_cpp
    assert '"test-device/debug"' not in main_cpp


def test_mac_suffix_explicit_topics_stay_literal(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Topics the user wrote by hand are never retargeted, even when they start with
    the device name; only topics synthesised from topic_prefix follow the MAC suffix."""
    main_cpp = generate_main(component_config_path("mac_suffix_explicit_topics.yaml"))

    assert "set_topic_prefix(App.get_name())" in main_cpp
    # will/shutdown are still defaulted from the prefix
    assert '(App.get_name() + "/status")' in main_cpp
    # log_topic and birth_message were configured explicitly, so they stay literal
    assert '.topic = "test-device/log",' in main_cpp
    assert '.topic = "test-device",' in main_cpp

    assert 'App.get_name() + "/log"' not in main_cpp
    assert 'App.get_name() + "/debug"' not in main_cpp


def test_mac_suffix_custom_prefix_stays_literal(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A topic_prefix set explicitly by the user is not the device name, so it and its
    defaulted topics stay compile-time literals with no App.get_name() involved."""
    main_cpp = generate_main(component_config_path("mac_suffix_custom_prefix.yaml"))

    assert 'set_topic_prefix("custom")' in main_cpp
    assert '"custom/status"' in main_cpp
    assert '"custom/debug"' in main_cpp

    assert "App.get_name()" not in main_cpp


def test_no_topic_prefix_disables_default_messages(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A null topic_prefix disables all the messages that would otherwise default from it."""
    main_cpp = generate_main(component_config_path("no_topic_prefix.yaml"))

    assert 'set_topic_prefix("")' in main_cpp
    assert "disable_birth_message()" in main_cpp
    assert "disable_last_will()" in main_cpp
    assert "disable_shutdown_message()" in main_cpp
    assert "disable_log_message()" in main_cpp


def test_no_mac_suffix_keeps_literal_topics(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without name_add_mac_suffix, topics stay plain compile-time strings."""
    main_cpp = generate_main(component_config_path("no_mac_suffix.yaml"))

    assert 'set_topic_prefix("test-device")' in main_cpp
    assert '"test-device/status"' in main_cpp
    assert '"test-device/debug"' in main_cpp

    assert "App.get_name()" not in main_cpp
