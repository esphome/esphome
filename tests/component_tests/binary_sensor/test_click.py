"""Tests for the click and double click trigger codegen."""

from collections.abc import Callable
from pathlib import Path
import re

from esphome.core import CORE


def test_click_triggers(generate_main: Callable[[str | Path], str]) -> None:
    """Each trigger gets its timing in milliseconds, defaults 50 and 350, and its own automation."""
    main_cpp = generate_main("tests/component_tests/binary_sensor/test_click.yaml")

    triggers = re.findall(
        r"new\((\w+)\) binary_sensor::(Click|DoubleClick)Trigger\(button_a, (\d+), (\d+)\);",
        main_cpp,
    )
    assert [trigger[1:] for trigger in triggers] == [
        ("Click", "50", "350"),
        ("Click", "1000", "3000"),
        ("DoubleClick", "20", "400"),
    ]
    for trigger_id, *_ in triggers:
        assert f"Automation<>({trigger_id});" in main_cpp
    assert "USE_BINARY_SENSOR_CLICK_TRIGGER" in {define.name for define in CORE.defines}
