"""Tests for the argument types of the ezo reply triggers."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

FORWARDER_RE = re.compile(r"ph_ezo->(add_\w+_callback)\(TriggerForwarder<([\w:]+)>")


@pytest.fixture
def forwarders(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> dict[str, list[str]]:
    main_cpp = generate_main(component_config_path("ezo_callbacks.yaml"))
    found: dict[str, list[str]] = {}
    for method, arg_type in FORWARDER_RE.findall(main_cpp):
        found.setdefault(method, []).append(arg_type)
    return found


def test_custom_callback_types(forwarders: dict[str, list[str]]) -> None:
    assert forwarders["add_custom_callback"] == ["StringRef", "std::string"]


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
    assert forwarders[method] == ["StringRef"]


def test_led_callback_type(forwarders: dict[str, list[str]]) -> None:
    assert forwarders["add_led_state_callback"] == ["bool"]
