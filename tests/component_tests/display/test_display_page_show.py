"""Tests for the code generated for the display.page.show action."""

from collections.abc import Callable

import pytest


@pytest.fixture
def main_cpp(generate_main: Callable[[str], str]) -> str:
    return generate_main("tests/component_tests/display/config/page_show.yaml")


def _function_body(main_cpp: str, signature: str) -> str:
    return main_cpp.split(f"{signature} {{\n", 1)[1].split("\n}\n", 1)[0]


def test_page_show_constant_page(main_cpp: str) -> None:
    """A page id is shown directly, with no action object."""
    assert "::page1->show();" in main_cpp
    assert "DisplayPageShowAction" not in main_cpp


def test_page_show_lambda_page(main_cpp: str) -> None:
    """A page lambda is called once and its page shown only when it is not null.

    The check is an if block, never an early return, so the actions after it still run.
    """
    body = _function_body(main_cpp, "static void esphome__applyaction_id__fn()")
    assert body.startswith(
        "  {\n"
        "  display::DisplayPage * esphome__page = []() -> display::DisplayPage * {\n"
    )
    assert "    return page2;\n" in body
    assert (
        "  }();\n"
        "  if (esphome__page != nullptr) {\n"
        "    esphome__page->show();\n"
        "  }\n"
        "  }\n"
        "  {\n"
        "  ::page1->show();\n"
        "  }"
    ) in body
    assert "return;" not in body


def test_page_show_callback_automation_is_direct(main_cpp: str) -> None:
    """A button press calls the generated function, with no Automation object."""
    assert (
        "show_page_button->add_on_press_callback([]() -> void {\n"
        "    esphome__applyaction_id__fn();\n"
        "});"
    ) in main_cpp
    assert "Automation<>" not in main_cpp


@pytest.mark.parametrize(
    "body",
    [
        "index *= 2; return index == 0 ? page1 : page2;",
        "return (index *= 2) == 0 ? page1 : page2;",
    ],
)
def test_page_show_lambda_takes_args_by_value(main_cpp: str, body: str) -> None:
    """The page lambda gets the trigger args by value, so it may modify them.

    It is called rather than reduced, so even a single return modifies only its own copy.
    """
    lambda_start = main_cpp.index(f"    {body}\n")
    head = main_cpp.rindex("[", 0, lambda_start)
    assert main_cpp[head:lambda_start].startswith(
        "[](int index) -> display::DisplayPage * {\n"
    )
    assert (
        f"    {body}\n    // ESPHOME_LINE_DIRECTIVE_RESET\n  }}(index);\n" in main_cpp
    )
