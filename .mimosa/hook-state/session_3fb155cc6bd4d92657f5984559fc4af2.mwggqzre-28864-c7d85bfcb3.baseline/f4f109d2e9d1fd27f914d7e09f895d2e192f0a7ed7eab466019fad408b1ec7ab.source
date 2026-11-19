"""Tests for the safe_mode component."""

from collections.abc import Callable

from esphome.core import CORE

SHUTDOWN_DEFINE = "USE_SAFE_MODE_BOOT_IS_GOOD_ON_SHUTDOWN"


def _has_define(name: str) -> bool:
    return any(define.name == name for define in CORE.defines)


def test_boot_is_good_on_shutdown_default(
    generate_main: Callable[[str], str],
) -> None:
    """By default, an orderly shutdown confirms the app image."""
    main_cpp = generate_main(
        "tests/component_tests/safe_mode/test_safe_mode_default.yaml"
    )

    assert "safe_mode::SafeModeComponent" in main_cpp
    assert _has_define(SHUTDOWN_DEFINE)


def test_boot_is_good_on_shutdown_disabled(
    generate_main: Callable[[str], str],
) -> None:
    """With boot_is_good_on_shutdown: false, the define is not added."""
    main_cpp = generate_main(
        "tests/component_tests/safe_mode/test_safe_mode_no_shutdown_confirm.yaml"
    )

    assert "safe_mode::SafeModeComponent" in main_cpp
    assert not _has_define(SHUTDOWN_DEFINE)


def test_safe_mode_disabled(generate_main: Callable[[str], str]) -> None:
    """With safe_mode disabled, no component and no define are generated."""
    main_cpp = generate_main(
        "tests/component_tests/safe_mode/test_safe_mode_disabled.yaml"
    )

    assert "safe_mode::SafeModeComponent" not in main_cpp
    assert not _has_define(SHUTDOWN_DEFINE)
