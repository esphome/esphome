"""Tests for the ``network: priority:`` list validator."""

import pytest
from voluptuous import Invalid

from esphome.components.network import (
    KEY_NETWORK_PRIORITY,
    NETWORK_PRIORITY_BASE,
    NETWORK_PRIORITY_STEP,
    _validate_priority_list,
    get_network_priority,
)
from esphome.core import CORE


@pytest.fixture(autouse=True)
def _clear_core_data():
    """Wipe CORE.data so each test starts with a clean slate."""
    CORE.data.clear()
    yield
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
