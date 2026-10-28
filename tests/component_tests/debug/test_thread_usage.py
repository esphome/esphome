"""Tests for the per-thread CPU usage statistics debug enables on nRF52 and platform: zephyr."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_PRJ_CONF

STATS_KEYS = ("CONFIG_THREAD_RUNTIME_STATS", "CONFIG_SCHED_THREAD_USAGE_ANALYSIS")


@pytest.mark.parametrize(
    ("fixture", "enabled"),
    [
        ("logger_debug.yaml", False),
        ("logger_verbose.yaml", True),
        ("logger_very_verbose.yaml", True),
        ("zephyr_logger_debug.yaml", False),
        ("zephyr_logger_verbose.yaml", True),
    ],
)
def test_thread_usage_follows_logger_level(
    fixture: str,
    enabled: bool,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The statistics are only built in when the verbose table can be logged."""
    generate_main(component_config_path(fixture))
    prj_conf = zephyr_data()[KEY_PRJ_CONF][""]
    for key in STATS_KEYS:
        assert (prj_conf.get(key, (False,))[0] is True) is enabled, key
