"""A value lambda in an lvgl action that changes a trigger arg gets its own copy of it.

The action's generated function takes the trigger args by const reference, so a lambda body
reduced inline against them would not compile.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

CONFIG = Path(__file__).parent / "config" / "action_value_lambda_copy_test.yaml"


CASES = [
    ("slider value", 1),
    ("canvas width", 2),
    ("set_pixels opa", 3),
    ("page time", 4),
    ("is_idle timeout", 5),
]


def test_value_lambda_takes_trigger_arg_by_value(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(CONFIG)
    assert "const std::remove_cvref_t<float> & x" in main_cpp
    for case, marker in CASES:
        # The lambda that runs `++x + <marker>` declares its own by-value `float x`
        pattern = (
            r"\]\(float x\b[^)]*\)[^{]*\{\s*(?:#line[^\n]*\n\s*)?"
            rf"return \+\+x \+ {marker};"
        )
        assert len(re.findall(pattern, main_cpp)) == 1, case
