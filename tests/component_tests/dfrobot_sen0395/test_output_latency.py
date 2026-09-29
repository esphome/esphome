"""Codegen tests for dfrobot_sen0395 output_latency units."""

from collections.abc import Callable
from pathlib import Path
import re


def _latency_return(main_cpp: str, setter: str) -> str:
    match = re.search(
        rf"{re.escape(setter)}\(\[\]\(\) -> float \{{\s*return ([^;]+);",
        main_cpp,
    )
    assert match is not None, f"{setter} not found in generated main"
    return match.group(1)


def test_output_latency_emits_seconds_not_milliseconds(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """YAML 0.1s / 5.0s must reach SetLatencyCommand as seconds, not milliseconds."""
    main_cpp = generate_main(component_config_path("output_latency.yaml"))

    assert _latency_return(main_cpp, "set_delay_after_detect") == "0.1f"
    assert _latency_return(main_cpp, "set_delay_after_disappear") == "5.0f"
