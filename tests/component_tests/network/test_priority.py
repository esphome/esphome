"""Tests for the ``network: priority:`` list validator."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest
from voluptuous import Invalid

from esphome.components.network import (
    _SETUP_PRIORITY_AFTER_WIFI,
    KEY_NETWORK_PRIORITY,
    NETWORK_PRIORITY_BASE,
    NETWORK_PRIORITY_STEP,
    _final_validate,
    _validate_priority_list,
    get_network_priority,
)
from esphome.const import CONF_PRIORITY
from esphome.core import CORE
import esphome.final_validate as fv


@pytest.fixture(autouse=True)
def _clear_core_data():
    """Wipe CORE.data and reset fv.full_config so each test starts clean."""
    CORE.data.clear()
    token = fv.full_config.set({})
    yield
    fv.full_config.reset(token)
    CORE.data.clear()


def test_validates_plain_string_list() -> None:
    result = _validate_priority_list(["ethernet", "wifi"])
    assert result == [{"interface": "ethernet"}, {"interface": "wifi"}]


def test_normalizes_mixed_case_to_lowercase() -> None:
    # Regression check: mixed-case input must be lowercased so downstream
    # callers like get_network_priority("ethernet") find a match.
    result = _validate_priority_list(["Ethernet", "WIFI"])
    assert result == [{"interface": "ethernet"}, {"interface": "wifi"}]


def test_accepts_all_supported_interface_types() -> None:
    # Only ethernet and wifi are currently accepted. Other interface types
    # (openthread, modem) will be added when their setup-priority consumers
    # land — see NETWORK_PLAN.md.
    result = _validate_priority_list(["ethernet", "wifi"])
    assert [e["interface"] for e in result] == ["ethernet", "wifi"]


def test_rejects_not_yet_supported_interface() -> None:
    # openthread / modem are in the long-term roadmap but no setup-priority
    # consumer is wired yet, so VALID_NETWORK_TYPES excludes them today.
    with pytest.raises(Invalid):
        _validate_priority_list(["ethernet", "openthread"])
    with pytest.raises(Invalid):
        _validate_priority_list(["wifi", "modem"])


def test_single_interface_is_valid() -> None:
    result = _validate_priority_list(["ethernet"])
    assert result == [{"interface": "ethernet"}]


def test_rejects_unknown_interface() -> None:
    with pytest.raises(Invalid):
        _validate_priority_list(["ethernet", "bluetooth"])


def test_rejects_duplicate_entries() -> None:
    with pytest.raises(Invalid, match="Duplicate entries"):
        _validate_priority_list(["ethernet", "ethernet"])


def test_rejects_duplicates_regardless_of_case() -> None:
    # Same interface in mixed cases should still trip the duplicate check
    # after normalization.
    with pytest.raises(Invalid, match="Duplicate entries"):
        _validate_priority_list(["ethernet", "Ethernet"])


def test_rejects_mapping_form() -> None:
    # The mapping form (- ethernet: { timeout: 30s }) was removed when the
    # timeout option moved to its consumer PR.  Verify we reject it cleanly
    # instead of silently accepting a no-op.
    with pytest.raises(Invalid):
        _validate_priority_list([{"ethernet": {"timeout": "30s"}}])


def test_get_network_priority_returns_none_when_unset() -> None:
    assert get_network_priority("ethernet") is None


def test_get_network_priority_assigns_base_to_first_entry() -> None:
    CORE.data[KEY_NETWORK_PRIORITY] = _validate_priority_list(["ethernet", "wifi"])
    assert get_network_priority("ethernet") == NETWORK_PRIORITY_BASE


def test_get_network_priority_steps_down_by_step_per_position() -> None:
    CORE.data[KEY_NETWORK_PRIORITY] = _validate_priority_list(["ethernet", "wifi"])
    assert get_network_priority("wifi") == NETWORK_PRIORITY_BASE - NETWORK_PRIORITY_STEP


def test_get_network_priority_is_case_insensitive_on_query() -> None:
    CORE.data[KEY_NETWORK_PRIORITY] = _validate_priority_list(["ethernet"])
    assert get_network_priority("Ethernet") == NETWORK_PRIORITY_BASE


def test_get_network_priority_returns_none_for_unlisted_interface() -> None:
    CORE.data[KEY_NETWORK_PRIORITY] = _validate_priority_list(["ethernet"])
    assert get_network_priority("wifi") is None


def test_final_validate_rejects_priority_iface_without_component() -> None:
    """An interface named in 'priority' with no matching component block is rejected."""
    # priority lists wifi, but only ethernet is present in the full config.
    fv.full_config.set({"ethernet": {}})
    config = {CONF_PRIORITY: _validate_priority_list(["ethernet", "wifi"])}
    with pytest.raises(
        Invalid, match=r"'wifi' is listed in 'network: priority:' but no 'wifi:'"
    ):
        _final_validate(config)


def test_final_validate_accepts_when_all_priority_ifaces_present() -> None:
    """No error when every interface in 'priority' has a matching component block."""
    fv.full_config.set({"ethernet": {}, "wifi": {}})
    config = {CONF_PRIORITY: _validate_priority_list(["ethernet", "wifi"])}
    _final_validate(config)  # must not raise


def test_final_validate_noop_without_priority_list() -> None:
    """A network config without a 'priority' list imposes no component requirements."""
    fv.full_config.set({})
    _final_validate({})  # must not raise


def _cpp_setup_priority(name: str) -> float:
    """Read a setup_priority constant straight from esphome/core/component.h."""
    header = Path(__file__).parents[3] / "esphome" / "core" / "component.h"
    match = re.search(
        rf"inline constexpr float {name} = ([\d.]+)f;", header.read_text()
    )
    assert match is not None, f"setup_priority::{name} not found in component.h"
    return float(match.group(1))


def test_priority_band_constants_match_cpp_setup_priority() -> None:
    """The Python priority-band constants mirror the C++ setup_priority values.

    NETWORK_PRIORITY_BASE must equal the historical setup_priority::WIFI /
    ::ETHERNET default so a single-entry priority list reproduces the legacy
    setup order, and the band guard must track setup_priority::AFTER_WIFI.
    Reading the values from component.h turns a silent desync into a CI
    failure if either side is ever rebalanced.
    """
    assert _cpp_setup_priority("WIFI") == NETWORK_PRIORITY_BASE
    assert _cpp_setup_priority("ETHERNET") == NETWORK_PRIORITY_BASE
    assert _cpp_setup_priority("AFTER_WIFI") == _SETUP_PRIORITY_AFTER_WIFI
    # Must stay below AFTER_BLUETOOTH (NetworkComponent's own priority) so
    # interfaces never set up before esp_netif_init().
    assert _cpp_setup_priority("AFTER_BLUETOOTH") > NETWORK_PRIORITY_BASE


def test_wifi_first_priority_emits_primary_interface_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A wifi-first priority list emits USE_NETWORK_PRIMARY_INTERFACE_WIFI."""
    generate_main(component_config_path("priority_wifi_first.yaml"))
    defines = {d.name for d in CORE.defines}
    assert "USE_NETWORK_PRIMARY_INTERFACE_WIFI" in defines
    # Emitted by cg.set_setup_priority() at the wifi/ethernet call sites.
    assert "USE_SETUP_PRIORITY_OVERRIDE" in defines


def test_ethernet_first_priority_emits_no_primary_interface_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Ethernet-first matches the built-in preference order, so no define is emitted."""
    generate_main(component_config_path("priority_ethernet_first.yaml"))
    assert not any(
        d.name.startswith("USE_NETWORK_PRIMARY_INTERFACE_") for d in CORE.defines
    )
    # The setup-priority overrides themselves are still emitted.
    assert "USE_SETUP_PRIORITY_OVERRIDE" in {d.name for d in CORE.defines}


def test_no_primary_interface_define_without_priority(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without a priority list, no primary-interface define is emitted."""
    generate_main(component_config_path("wifi_only.yaml"))
    assert not any(
        d.name.startswith("USE_NETWORK_PRIMARY_INTERFACE_") for d in CORE.defines
    )
