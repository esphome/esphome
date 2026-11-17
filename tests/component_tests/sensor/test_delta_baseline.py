"""Tests for the sensor delta filter baseline."""

from collections.abc import Callable
import re


def test_delta_baseline_number(generate_main: Callable[[str], str]) -> None:
    """A plain number baseline, including 0, becomes a lambda returning it."""
    main_cpp = generate_main("tests/component_tests/sensor/test_delta_baseline.yaml")

    # A user lambda body also carries #line directives around the return.
    for body in (r"return 20\.0f;", r"return 0\.0f;", r"return x \* 2\.0f;"):
        assert re.search(
            rf"set_baseline\(\[\]\(float x\) -> float \{{[^}}]*?{body}[^}}]*\}}\);",
            main_cpp,
        ), body
