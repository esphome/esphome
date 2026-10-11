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
    merged = re.search(r"merged->set_filters\(\{(\w+)\}\);", main_cpp)
    assert merged is not None
    assert f"new({merged.group(1)}) sensor::TimeoutThrottleFilter(1000);" in main_cpp
    # Every other shape keeps its two filters
    for sensor_id in ("other_periods", "other_order", "value_list", "configured_value"):
        call = re.search(rf"{sensor_id}->set_filters\(\{{([^}}]*)\}}\);", main_cpp)
        assert call is not None and len(call.group(1).split(",")) == 2, sensor_id
