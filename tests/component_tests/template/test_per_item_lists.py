"""Filters and action lists are added one item at a time, so no pointer list lands in .rodata."""

from collections.abc import Callable
from pathlib import Path
import re


def test_filters_and_actions_are_added_per_item(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("per_item_lists.yaml"))

    for list_call in (
        "add_filters(",
        "set_filters(",
        "add_actions(",
        "add_then(",
        "add_else(",
    ):
        assert list_call not in main_cpp
    assert len(re.findall(r"temp->add_filter\(", main_cpp)) == 3
    assert len(re.findall(r"button->add_filter\(", main_cpp)) == 2
    assert len(re.findall(r"label->add_filter\(", main_cpp)) == 2
    assert main_cpp.count("->add_then_action(") == 4
    assert main_cpp.count("->add_else_action(") == 1
    assert main_cpp.count("->finish_then();") == 3
    assert main_cpp.count("->finish_else();") == 1
    # and/or conditions and the sensor `or` filter take their items as separate arguments
    assert not re.search(r"(Condition|OrFilter)<[^>]*>\(\{", main_cpp)
