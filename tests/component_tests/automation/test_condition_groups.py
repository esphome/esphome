"""Tests for and/or condition group codegen."""

from collections.abc import Callable
from pathlib import Path


def test_single_condition_groups_are_unwrapped(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A group of one condition is passed to the action directly."""
    main_cpp = generate_main(
        "tests/component_tests/automation/test_condition_groups.yaml"
    )

    assert "AndCondition<1" not in main_cpp
    assert "OrCondition<1" not in main_cpp
    assert "IfAction<false>(lambdacondition_id);" in main_cpp
    assert "IfAction<false>(lambdacondition_id_2);" in main_cpp
    assert "AndCondition<2>({lambdacondition_id_3, lambdacondition_id_4});" in main_cpp
    assert "IfAction<false>(andcondition_id_2);" in main_cpp
