"""Each MPR121 hub keeps its channels in a list sized for the busiest hub."""

from collections.abc import Callable
from pathlib import Path

from ..helpers import get_define_value


def test_channel_list_is_sized_for_the_busiest_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("three_hubs.yaml"))
    # 3, 1 and 0 channels: the largest per hub count, not the sum
    assert get_define_value("MPR121_CHANNEL_COUNT") == "3"
    for hub, channel in (
        ("hub_busy", "busy_0"),
        ("hub_one", "one_0"),
        ("hub_busy", "busy_1"),
        ("hub_busy", "busy_2"),
    ):
        assert f"{hub}->register_channel({channel});" in main_cpp
    assert "hub_none->register_channel(" not in main_cpp
