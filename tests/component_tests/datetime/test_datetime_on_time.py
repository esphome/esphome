"""Tests for the code generated for datetime on_time blocks."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from tests.component_tests.helpers import callback_function, constructed


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path("datetime_on_time.yaml"))


@pytest.mark.parametrize(
    ("var", "base", "parent"),
    [
        ("datetime_ontimebase_id", "OnTimeBase", "test_time"),
        ("datetime_ondatetimebase_id", "OnDateTimeBase", "test_datetime"),
    ],
)
def test_on_time_constructed(main_cpp: str, var: str, base: str, parent: str) -> None:
    """Each on_time block is a callback component built on its function and given its entity."""
    function = callback_function(main_cpp, var)
    assert (
        constructed(main_cpp, var)
        == f"FireCallback<{function}, datetime::{base}>({parent})"
    )
    assert re.search(rf"App\.register_component_\({var}, \d+\);", main_cpp)
    assert f"{var}->set_parent(" not in main_cpp
