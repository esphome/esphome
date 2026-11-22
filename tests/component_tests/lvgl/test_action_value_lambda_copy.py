"""A value lambda in an lvgl action that changes a trigger arg gets its own copy of it.

The action's generated function takes the trigger args by const reference, so a lambda body
reduced inline against them would not compile.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

CONFIG = Path(__file__).parent / "config" / "action_value_lambda_copy_test.yaml"


def test_value_lambda_takes_trigger_arg_by_value(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(CONFIG)
    assert "const std::remove_cvref_t<float> & x" in main_cpp
    # Each lambda that runs `++x` (slider value, canvas width) declares its own by-value `float x`
    copies = re.findall(
        r"\]\(float x\b[^)]*\)[^{]*\{\s*(?:#line[^\n]*\n\s*)?return \+\+x;", main_cpp
    )
    assert len(copies) == 2
