"""Unit tests for esphome.components.esp32."""

from esphome.components.esp32 import _ota_downgrade_protection_errors


def test_downgrade_protection_passes_with_numeric_version_and_signing() -> None:
    assert _ota_downgrade_protection_errors("1.2.3", signed_ota_enabled=True) == []


def test_downgrade_protection_requires_project_version() -> None:
    errs = _ota_downgrade_protection_errors(None, signed_ota_enabled=True)
    assert len(errs) == 1
    assert "version" in str(errs[0])


def test_downgrade_protection_rejects_non_numeric_version() -> None:
    errs = _ota_downgrade_protection_errors("1.0-beta", signed_ota_enabled=True)
    assert len(errs) == 1
    assert "dotted-numeric" in str(errs[0])


def test_downgrade_protection_requires_signed_ota() -> None:
    errs = _ota_downgrade_protection_errors("1.2.3", signed_ota_enabled=False)
    assert len(errs) == 1
    assert "signed_ota_verification" in str(errs[0])


def test_downgrade_protection_reports_all_unmet_requirements() -> None:
    # No project version and no signing -> two distinct errors.
    errs = _ota_downgrade_protection_errors(None, signed_ota_enabled=False)
    assert len(errs) == 2


def test_downgrade_protection_accepts_calendar_version() -> None:
    assert _ota_downgrade_protection_errors("2024.12.0", signed_ota_enabled=True) == []
