"""Tests for the argument types of the nextion custom-protocol triggers."""

from collections.abc import Callable
from pathlib import Path


def test_generated_automation_types(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("custom_triggers.yaml"))

    # A synchronous automation is a direct function that needs no Automation
    assert (
        "const std::remove_cvref_t<StringRef> & key, "
        "const std::remove_cvref_t<StringRef> & value" in main_cpp
    )
    assert "Automation<std::string, std::string>" in main_cpp
