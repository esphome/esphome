"""Tests for MQTT topic generation with name_add_mac_suffix."""

from collections.abc import Callable
from pathlib import Path


def test_mac_suffix_default_topics(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Default topics must be rebuilt from App.get_name() so they carry the MAC suffix."""
    main_cpp = generate_main(
        "tests/component_tests/mqtt/config/mac_suffix_default_topics.yaml"
    )

    assert "set_topic_prefix(App.get_name())" in main_cpp
    assert '(App.get_name() + "/status")' in main_cpp
    assert '(App.get_name() + "/debug")' in main_cpp

    assert '"test-device/status"' not in main_cpp
    assert '"test-device/debug"' not in main_cpp


def test_mac_suffix_custom_topics(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A prefix that is not the bare device name stays a literal; a topic equal to the
    device name is rebuilt as App.get_name() with no concatenation."""
    main_cpp = generate_main(
        "tests/component_tests/mqtt/config/mac_suffix_custom_topics.yaml"
    )

    # topic_prefix does not equal (and is not "<name>/...") the device name, so it stays literal
    assert 'set_topic_prefix("test-device2")' in main_cpp
    # log_topic starts with "<name>/" so it is rebuilt from App.get_name()
    assert '(App.get_name() + "/log")' in main_cpp
    # birth/will topics are unrelated to the device name, so they stay literal
    assert '"other/status"' in main_cpp
    # shutdown topic equals the bare device name, so it becomes App.get_name() with no concatenation
    assert ".topic = App.get_name()," in main_cpp


def test_no_mac_suffix_keeps_literal_topics(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Without name_add_mac_suffix, topics stay plain compile-time strings."""
    main_cpp = generate_main("tests/component_tests/mqtt/config/no_mac_suffix.yaml")

    assert 'set_topic_prefix("test-device")' in main_cpp
    assert '"test-device/status"' in main_cpp
    assert '"test-device/debug"' in main_cpp

    assert "App.get_name() +" not in main_cpp
