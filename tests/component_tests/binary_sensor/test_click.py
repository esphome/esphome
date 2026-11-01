"""Tests for the click and double click trigger codegen."""

from collections.abc import Callable
from pathlib import Path
import re

from esphome.core import CORE


def test_click_triggers(generate_main: Callable[[str | Path], str]) -> None:
    """Each trigger gets a detector and a callback with its timing in milliseconds, defaults 50 and 350."""
    main_cpp = generate_main("tests/component_tests/binary_sensor/test_click.yaml")

    kinds = dict(
        re.findall(
            r"new\((\w+)\) binary_sensor::(Click|DoubleClick)Detector\(\);", main_cpp
        )
    )
    calls = re.findall(r"if \(!\(::(\w+)->check\(state, (\d+), (\d+)\)\)\)", main_cpp)
    assert [(kinds[detector_id], *timing) for detector_id, *timing in calls] == [
        ("Click", "50", "350"),
        ("Click", "1000", "3000"),
        ("DoubleClick", "20", "400"),
    ]
    assert main_cpp.count("button_a->add_on_state_callback([](") == 3
    # Only the automation with a delay keeps an Automation object
    assert re.findall(r"new\((\w+)\) Automation<>\(\);", main_cpp) == [
        "automation_id_2"
    ]
    assert "::automation_id_2->trigger();" in main_cpp
    assert "USE_BINARY_SENSOR_CLICK_TRIGGER" in {define.name for define in CORE.defines}
