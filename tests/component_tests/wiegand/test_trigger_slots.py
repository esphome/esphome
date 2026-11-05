"""Each Wiegand reader keeps its automations in lists sized for the busiest reader."""

from collections.abc import Callable
from pathlib import Path

from ..helpers import get_define_value


def test_automation_lists_are_sized_for_the_busiest_reader(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("three_hubs.yaml"))
    # 3, 1 and 0 key automations: the largest per reader count, not the sum
    assert get_define_value("WIEGAND_ON_KEY_COUNT") == "3"
    assert main_cpp.count("hub_busy->add_on_key_automation(") == 3
    assert main_cpp.count("hub_one->add_on_key_automation(") == 1
    assert "hub_none->add_on_key_automation(" not in main_cpp
    # no reader has tag or raw automations, so those lists are compiled out
    assert get_define_value("WIEGAND_ON_TAG_COUNT") is None
    assert get_define_value("WIEGAND_ON_RAW_COUNT") is None
