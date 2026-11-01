"""Each Wiegand reader keeps its triggers in lists sized for the busiest reader."""

from collections.abc import Callable
from pathlib import Path

from ..helpers import get_define_value


def test_trigger_lists_are_sized_for_the_busiest_reader(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("three_hubs.yaml"))
    # 3, 1 and 0 key triggers: the largest per reader count, not the sum
    assert get_define_value("WIEGAND_KEY_TRIGGER_COUNT") == "3"
    # no reader has tag or raw triggers, so those lists are empty
    assert get_define_value("WIEGAND_TAG_TRIGGER_COUNT") == "0"
    assert get_define_value("WIEGAND_RAW_TRIGGER_COUNT") == "0"
    assert main_cpp.count("hub_busy->register_key_trigger(") == 3
    assert main_cpp.count("hub_one->register_key_trigger(") == 1
    assert "hub_none->register_key_trigger(" not in main_cpp
