"""Tests for MQTT topic generation with name_add_mac_suffix."""

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.mark.parametrize(
    ("config_file", "present", "absent"),
    [
        pytest.param(
            "mac_suffix_default_topics.yaml",
            (
                "set_topic_prefix(App.get_name())",
                '(App.get_name() + "/status")',
                '(App.get_name() + "/debug")',
            ),
            ('"test-device/status"', '"test-device/debug"'),
            id="default topics follow the MAC suffix",
        ),
        pytest.param(
            "mac_suffix_explicit_topics.yaml",
            (
                "set_topic_prefix(App.get_name())",
                # will and shutdown are still defaulted from the prefix
                '(App.get_name() + "/status")',
                '.topic = "test-device/log",',
                '.topic = "test-device",',
            ),
            ('App.get_name() + "/log"', 'App.get_name() + "/debug"'),
            id="explicit topics stay literal",
        ),
        pytest.param(
            "mac_suffix_default_written_out.yaml",
            ('(App.get_name() + "/status")', '(App.get_name() + "/debug")'),
            ('"test-device/status"', '"test-device/debug"'),
            id="topics written out as the default follow the MAC suffix",
        ),
        pytest.param(
            "mac_suffix_custom_prefix.yaml",
            ('set_topic_prefix("custom")', '"custom/status"', '"custom/debug"'),
            ("App.get_name()",),
            id="custom prefix stays literal",
        ),
        pytest.param(
            "no_mac_suffix.yaml",
            (
                'set_topic_prefix("test-device")',
                '"test-device/status"',
                '"test-device/debug"',
            ),
            ("App.get_name()",),
            id="no MAC suffix",
        ),
        pytest.param(
            "no_topic_prefix.yaml",
            (
                'set_topic_prefix("")',
                "disable_birth_message()",
                "disable_last_will()",
                "disable_shutdown_message()",
                "disable_log_message()",
            ),
            (),
            id="null prefix disables default messages",
        ),
    ],
)
def test_topics(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    present: tuple[str, ...],
    absent: tuple[str, ...],
) -> None:
    main_cpp = generate_main(component_config_path(config_file))

    for text in present:
        assert text in main_cpp
    for text in absent:
        assert text not in main_cpp
