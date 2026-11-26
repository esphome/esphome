"""Codegen of the sensor on_value_range trigger."""

from collections.abc import Callable
from pathlib import Path


def test_bound_can_use_its_own_trigger(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The component is declared before its bounds, so a bound lambda can use its id."""
    main_cpp = generate_main(component_config_path("value_range_self_reference.yaml"))

    assert (
        "new(range_trigger) FireCallback<esphome__automation_id__fn, sensor::ValueRangeBase>("
        "source, esphome__range_trigger__fn);" in main_cpp
    )
    assert "range_trigger->is_ready()" in main_cpp
