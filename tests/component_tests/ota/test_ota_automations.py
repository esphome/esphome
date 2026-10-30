"""Tests for the on_* automations every OTA platform gets from ota_to_code."""

from __future__ import annotations

from collections.abc import Callable

from esphome.core import CORE

AUTOMATIONS_YAML = "tests/component_tests/ota/test_ota_automations.yaml"
PLAIN_YAML = "tests/component_tests/ota/test_esphome_ota_plain.yaml"


def _code(main_cpp: str) -> str:
    """The generated C++ without the YAML that generate_main echoes as comments."""
    return "\n".join(
        line for line in main_cpp.splitlines() if not line.lstrip().startswith("//")
    )


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def test_ota_automations_generated(generate_main: Callable[[str], str]) -> None:
    """Every key builds its automation on every platform and compiles in the
    state notifications."""
    main_cpp = _code(generate_main(AUTOMATIONS_YAML))
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
        "http end",
    ):
        assert message in main_cpp
    assert "USE_OTA_STATE_LISTENER" in _defines()


def test_ota_automations_are_callbacks(generate_main: Callable[[str], str]) -> None:
    """Each automation is one callback on the list every OTA component shares,
    filtered on its component and state; the list is sized for all of them."""
    main_cpp = generate_main(AUTOMATIONS_YAML)
    assert main_cpp.count("ota::OTAComponent::add_state_callback(") == 9
    assert main_cpp.count("component == ::ota_native") == 7
    assert main_cpp.count("component == ::ota_web") == 2
    assert "component == ::ota_http" not in main_cpp
    assert main_cpp.count("!component->is_failed()") == 9
    assert "Trigger" not in main_cpp
    assert "->trigger(progress);" in main_cpp
    assert "->trigger(error);" in main_cpp
    assert "->trigger(state);" in main_cpp
    counts = {d.name: str(d.value) for d in CORE.defines}
    assert counts["OTA_STATE_CALLBACK_COUNT"] == "9"


def test_ota_without_automations_skips_state_listener(
    generate_main: Callable[[str], str],
) -> None:
    """Without automations or listeners the state notifications compile out."""
    generate_main(PLAIN_YAML)
    assert not {"USE_OTA_STATE_LISTENER", "OTA_STATE_CALLBACK_COUNT"} & _defines()
