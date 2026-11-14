"""An explicit ``scrollable: true`` on a container also makes it clickable, so a drag
on its background scrolls it.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path("container_scrollable_test.yaml"))


def _set_clickable_calls(main_cpp: str, widget_id: str) -> list[str]:
    prefix = f"lv_obj_set_clickable({widget_id}, "
    return [
        line.strip().removeprefix(prefix).removesuffix(");")
        for line in main_cpp.splitlines()
        if line.strip().startswith(prefix)
    ]


@pytest.mark.parametrize(
    ("widget_id", "expected"),
    [
        ("cont_scrollable", ["true"]),
        ("cont_default", []),
        ("cont_not_scrollable", []),
        ("cont_scrollable_not_clickable", ["false"]),
        ("cont_scrollable_lambda", []),
        ("obj_scrollable", []),
    ],
)
def test_container_set_clickable(
    main_cpp: str, widget_id: str, expected: list[str]
) -> None:
    assert _set_clickable_calls(main_cpp, widget_id) == expected
