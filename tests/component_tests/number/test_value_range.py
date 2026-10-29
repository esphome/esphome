"""Codegen of the number on_value_range trigger."""

from collections.abc import Callable
from pathlib import Path


def test_bound_can_use_its_own_trigger(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The trigger is declared before its bounds, so a bound lambda can use the trigger's id."""
    main_cpp = generate_main(component_config_path("value_range_self_reference.yaml"))

    assert (
        "new(range_trigger) number::ValueRangeTrigger(source, esphome__range_trigger__fn);"
        in main_cpp
    )
    assert "range_trigger->is_action_running()" in main_cpp
