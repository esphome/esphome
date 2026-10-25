"""Tests for the toptronic preset corpus loading and directory mapping."""

import logging
from pathlib import Path

import pytest

from esphome.components import toptronic
from esphome.components.toptronic import _preset_device_type


@pytest.mark.parametrize(
    ("value", "expected"), [("BD", "BM"), ("bd", "BM"), ("hv", "HV"), ("HV", "HV")]
)
def test_preset_device_type_before_validation(value: str, expected: str) -> None:
    assert _preset_device_type({"device_type": value}) == expected


def _write(directory: Path, name: str, platform: str, entity_name: str) -> None:
    (directory / name).write_text(
        f"{platform}:\n  - name: {entity_name}\n", encoding="utf-8"
    )


def _presets(tmp_path: Path, monkeypatch, files: dict[str, tuple[str, str]]) -> None:
    """Point the loader at a temporary corpus."""
    directory = tmp_path / "HV"
    directory.mkdir()
    for name, (platform, entity_name) in files.items():
        _write(directory, name, platform, entity_name)
    monkeypatch.setattr(toptronic, "PRESETS_DIR", tmp_path)


def _platforms(entities: list[tuple[str, dict]]) -> set[str]:
    return {platform for platform, _ in entities}


def test_english_corpus_loads_without_a_fallback(tmp_path, monkeypatch, caplog) -> None:
    caplog.set_level(logging.WARNING)
    _presets(
        tmp_path,
        monkeypatch,
        {
            "sensors_en.yaml": ("sensor", "Temperature"),
            "buttons_en.yaml": ("button", "Acknowledge"),
        },
    )
    entities = toptronic._load_entities("HV", "en")
    assert _platforms(entities) == {"sensor", "button"}
    assert not caplog.records


def test_each_kind_falls_back_to_english_on_its_own(
    tmp_path, monkeypatch, caplog
) -> None:
    """A translated button does not keep an untranslated sensor out of the corpus."""
    caplog.set_level(logging.WARNING)
    _presets(
        tmp_path,
        monkeypatch,
        {
            # Only the buttons are translated
            "sensors_en.yaml": ("sensor", "Temperature"),
            "buttons_de.yaml": ("button", "Quittieren"),
        },
    )
    entities = toptronic._load_entities("HV", "de")
    assert _platforms(entities) == {"sensor", "button"}
    names = {entry["name"] for _, entry in entities}
    assert names == {"Temperature", "Quittieren"}
    # Only the missing sensors file is reported
    fallbacks = [record.getMessage() for record in caplog.records]
    assert len(fallbacks) == 1
    assert "sensors presets" in fallbacks[0]
    assert "'de'" in fallbacks[0]


def test_device_type_without_presets_yields_nothing(
    tmp_path, monkeypatch, caplog
) -> None:
    caplog.set_level(logging.WARNING)
    (tmp_path / "HV").mkdir()
    monkeypatch.setattr(toptronic, "PRESETS_DIR", tmp_path)
    assert toptronic._load_entities("HV", "de") == []
    # Nothing to fall back to, so this is not a missing-translation warning
    assert not caplog.records
