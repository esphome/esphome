"""Tests for the code generated for the display.page.show action."""

from collections.abc import Callable

import pytest


@pytest.fixture
def main_cpp(generate_main: Callable[[str], str]) -> str:
    return generate_main("tests/component_tests/display/config/page_show.yaml")


def test_page_show_constant_page(main_cpp: str) -> None:
    """A page id is wrapped in a lambda that returns the page."""
    assert (
        "new(display_displaypageshowaction_id_2) display::DisplayPageShowAction<>();\n"
        "display_displaypageshowaction_id_2->set_page([]() -> display::DisplayPage * {\n"
        "    return page1;\n"
        "});"
    ) in main_cpp


def test_page_show_lambda_page(main_cpp: str) -> None:
    """A page lambda is stored on the action."""
    assert (
        "display_displaypageshowaction_id->set_page([]() -> display::DisplayPage * {\n"
    ) in main_cpp
    assert "    return page2;\n" in main_cpp


@pytest.mark.parametrize(
    "body",
    [
        "index *= 2; return index == 0 ? page1 : page2;",
        "return (index *= 2) == 0 ? page1 : page2;",
    ],
)
def test_page_show_lambda_takes_args_by_value(main_cpp: str, body: str) -> None:
    """The page lambda gets the trigger args by value, so it may modify them."""
    lambda_start = main_cpp.index(f"    {body}\n")
    head = main_cpp.rindex("[", 0, lambda_start)
    assert main_cpp[head:lambda_start].startswith(
        "[](int index) -> display::DisplayPage * {\n"
    )
