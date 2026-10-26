"""Tests for finding a hub's preset directory."""

import pytest
import yaml

from esphome.components.toptronic import AUTO_LOAD, PRESETS_DIR, _preset_device_type


@pytest.mark.parametrize(
    ("value", "expected"), [("BD", "BM"), ("bd", "BM"), ("hv", "HV"), ("HV", "HV")]
)
def test_preset_device_type_before_validation(value: str, expected: str) -> None:
    assert _preset_device_type({"device_type": value}) == expected


def test_lowercase_device_type_loads_preset_buttons() -> None:
    assert "button" in AUTO_LOAD([{"device_type": "hv"}])


def test_shipped_presets_do_not_hard_code_ids() -> None:
    """A preset id is build-wide, so a second hub of the same type would reuse it."""
    for path in sorted(PRESETS_DIR.glob("*/*.yaml")):
        data = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
        for entries in data.values():
            for entry in entries or []:
                assert "id" not in entry, (
                    f"{path.name}: {entry.get('name')!r} sets an id, which a second hub "
                    f"would duplicate"
                )
