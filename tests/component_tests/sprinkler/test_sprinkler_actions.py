"""Tests for the sprinkler action codegen."""

from collections.abc import Callable
from pathlib import Path


def test_sprinkler_actions_call_the_controller(
    generate_main: Callable[[str | Path], str],
) -> None:
    """start_single_valve passes 0 (the valve's own run duration) when none is set; shutdown and
    resume_or_start_full_cycle call the controller directly."""
    main_cpp = generate_main(
        "tests/component_tests/sprinkler/test_sprinkler_actions.yaml"
    )

    assert "::ctrl->start_single_valve(1, 0);" in main_cpp
    assert "::ctrl->start_single_valve(0, 600);" in main_cpp
    assert "::ctrl->shutdown();" in main_cpp
    assert "::ctrl->resume_or_start_full_cycle();" in main_cpp
