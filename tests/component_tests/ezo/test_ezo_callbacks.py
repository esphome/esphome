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
def forwarders(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> dict[str, list[str]]:
    main_cpp = generate_main(component_config_path("ezo_callbacks.yaml"))
    found: dict[str, list[str]] = {}
    for method, forwarded, direct in CALLBACK_RE.findall(main_cpp):
        found.setdefault(method, []).append(forwarded or direct)
    return found


def test_custom_callback_types(forwarders: dict[str, list[str]]) -> None:
    assert forwarders["add_custom_callback"] == ["std::string", "std::string"]


@pytest.mark.parametrize(
    "method",
    [
        "add_calibration_callback",
        "add_slope_callback",
        "add_t_callback",
        "add_device_infomation_callback",
    ],
)
def test_reply_callback_types(forwarders: dict[str, list[str]], method: str) -> None:
    assert forwarders[method] == ["std::string"]


def test_led_callback_type(forwarders: dict[str, list[str]]) -> None:
    assert forwarders["add_led_state_callback"] == ["bool"]
