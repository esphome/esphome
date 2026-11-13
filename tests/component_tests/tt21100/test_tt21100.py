"""Tests for the tt21100 key binary sensor codegen."""

from collections.abc import Callable
from pathlib import Path
import re

from esphome.core import CORE


def test_buttons_are_added_sorted_by_index(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Each hub gets its buttons by index, in config order within an index."""
    main_cpp = generate_main(component_config_path("buttons.yaml"))

    adds = re.findall(r"(ts\d)->add_button_binary_sensor\((\w+), (\d)\);", main_cpp)
    assert [a for a in adds if a[0] == "ts1"] == [
        ("ts1", "key0", "0"),
        ("ts1", "key0_again", "0"),
        ("ts1", "key3", "3"),
    ]
    assert [a for a in adds if a[0] == "ts2"] == [("ts2", "other0", "0")]
    defines = {d.name: str(d.value) for d in CORE.defines}
    assert defines["TT21100_BUTTON_COUNT"] == "3"
