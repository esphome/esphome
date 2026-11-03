"""timeout (value last) followed by throttle_with_priority with the same period becomes one filter."""

from collections.abc import Callable
from pathlib import Path
import re


def test_timeout_throttle_pair_is_merged(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main("tests/component_tests/sensor/timeout_throttle.yaml")

    # Only the `merged` sensor's adjacent pair becomes one object
    assert (
        len(re.findall(r"new\(\w+\) sensor::TimeoutThrottleFilter\(1000\);", main_cpp))
        == 1
    )
    merged = re.findall(r"merged->add_filter\((\w+)\);", main_cpp)
    assert len(merged) == 1
    assert f"new({merged[0]}) sensor::TimeoutThrottleFilter(1000);" in main_cpp
    # Every other shape keeps its two filters
    for sensor_id in ("other_periods", "other_order", "value_list", "configured_value"):
        calls = re.findall(rf"{sensor_id}->add_filter\(\w+\);", main_cpp)
        assert len(calls) == 2, sensor_id
