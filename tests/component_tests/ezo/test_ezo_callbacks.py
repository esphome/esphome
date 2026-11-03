"""Tests for the argument types of the ezo reply triggers."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

# A synchronous automation is a direct lambda, a deferring one a TriggerForwarder
CALLBACK_RE = re.compile(
    r"ph_ezo->(add_\w+_callback)\("
    r"(?:TriggerForwarder<([\w:]+)>|\[\]\(const std::remove_cvref_t<([\w:]+)> &)"
)


@pytest.fixture
def callbacks(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> dict[str, list[tuple[str, str]]]:
    main_cpp = generate_main(component_config_path("ezo_callbacks.yaml"))
    found: dict[str, list[tuple[str, str]]] = {}
    for method, forwarded, direct in CALLBACK_RE.findall(main_cpp):
        path = "forwarder" if forwarded else "direct"
        found.setdefault(method, []).append((path, forwarded or direct))
    return found


def test_custom_callback_types(callbacks: dict[str, list[tuple[str, str]]]) -> None:
    assert callbacks["add_custom_callback"] == [
        ("direct", "StringRef"),
        ("forwarder", "std::string"),
    ]


@pytest.mark.parametrize(
    "method",
    [
        "add_calibration_callback",
        "add_slope_callback",
        "add_t_callback",
        "add_device_infomation_callback",
    ],
)
def test_reply_callback_types(
    callbacks: dict[str, list[tuple[str, str]]], method: str
) -> None:
    assert callbacks[method] == [("direct", "StringRef")]


def test_led_callback_type(callbacks: dict[str, list[tuple[str, str]]]) -> None:
    assert callbacks["add_led_state_callback"] == [("direct", "bool")]
