"""Tests for the on_* automations every OTA platform gets from ota_to_code."""

from __future__ import annotations

from collections.abc import Callable

from esphome.core import CORE

AUTOMATIONS_YAML = "tests/component_tests/ota/test_ota_automations.yaml"
PLAIN_YAML = "tests/component_tests/ota/test_esphome_ota_plain.yaml"


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def test_ota_automations_generated(generate_main: Callable[[str], str]) -> None:
    """Every key builds its automation on every platform and compiles in the
    state notifications."""
    main_cpp = generate_main(AUTOMATIONS_YAML)
    for message in (
        "native begin",
        "native progress %.1f",
        "native end",
        "native abort",
        "native error %u",
        "native state %d",
        "native second state",
        "web begin",
        "web abort",
    ):
        assert message in main_cpp
    assert "USE_OTA_STATE_LISTENER" in _defines()


def test_ota_without_automations_skips_state_listener(
    generate_main: Callable[[str], str],
) -> None:
    """Without automations or listeners the state notifications compile out."""
    generate_main(PLAIN_YAML)
    assert "USE_OTA_STATE_LISTENER" not in _defines()
