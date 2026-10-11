"""Tests for and/or condition group codegen."""

from collections.abc import Callable
from pathlib import Path
import re


def test_single_condition_groups_are_unwrapped(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A group of one condition is passed to the action directly."""
    main_cpp = generate_main(component_config_path("condition_groups.yaml"))

    assert "IfAction<false>(lambdacondition_id);" in main_cpp
    assert "IfAction<false>(lambdacondition_id_2);" in main_cpp
    group = re.search(
        r"new\((\w+)\) AndCondition<2>\(\{lambdacondition_id_3, lambdacondition_id_4\}\);",
        main_cpp,
    )
    assert group is not None
    assert f"IfAction<false>({group.group(1)});" in main_cpp
    # xor of one condition equals that condition, so it is unwrapped too.
    assert "IfAction<false>(lambdacondition_id_5);" in main_cpp
    assert "XorCondition<" not in main_cpp
