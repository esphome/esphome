"""Tests for the bundled toptronic preset corpus."""

from pathlib import Path

import pytest
import yaml

import esphome

PRESETS = Path(esphome.__file__).parent / "components" / "toptronic" / "presets"
LANGS = ("de", "en", "fr", "it")
KINDS = ("sensors", "inputs", "buttons")


def _preset_files():
    """Yield (device_type, kind, language, path) for every preset file."""
    for path in sorted(PRESETS.glob("*/*.yaml")):
        device_type = path.parent.name
        kind, _, language = path.stem.rpartition("_")
        yield device_type, kind, language, path


def _entries(path: Path):
    """Return (platform, entity_config) pairs from one preset file."""
    data = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
    return [
        (platform, entry)
        for platform, entries in data.items()
        for entry in entries or []
    ]


def _key(platform: str, entry: dict) -> tuple:
    return (
        platform,
        entry.get("function_group"),
        entry.get("function_number"),
        entry.get("datapoint"),
    )


ALL_FILES = list(_preset_files())


def test_corpus_is_present() -> None:
    assert ALL_FILES, f"no preset files found under {PRESETS}"


@pytest.mark.parametrize(
    ("device_type", "kind", "language", "path"),
    ALL_FILES,
    ids=[f"{d}/{k}_{lang}" for d, k, lang, _ in ALL_FILES],
)
def test_entity_metadata(device_type, kind, language, path) -> None:
    """Each entry names its own device type and keeps options/values aligned."""
    for _, entry in _entries(path):
        assert entry.get("device_type") == device_type, (
            f"{path.name}: entry {entry.get('name')!r} declares device_type "
            f"{entry.get('device_type')!r}, expected {device_type!r}"
        )
        options = entry.get("options")
        values = entry.get("values")
        if options is None:
            continue
        assert values is not None, f"{path.name}: options without values"
        assert len(options) == len(values), (
            f"{path.name}: {len(options)} options vs {len(values)} values "
            f"for {entry.get('name')!r}"
        )
        assert len(set(values)) == len(values), (
            f"{path.name}: duplicate values for {entry.get('name')!r}"
        )


def test_languages_are_complete() -> None:
    """A kind shipped in one language is shipped in all of them."""
    present = {
        (device_type, kind, language) for device_type, kind, language, _ in ALL_FILES
    }
    missing = [
        (device_type, kind, language)
        for device_type, kind, _ in present
        for language in LANGS
        if (device_type, kind, language) not in present
    ]
    assert not missing, f"missing preset translations: {missing}"


def test_entity_keys_match_across_languages() -> None:
    """Every language offers the same datapoints with the same option counts."""
    groups: dict[tuple[str, str], dict[str, dict]] = {}
    for device_type, kind, language, path in ALL_FILES:
        groups.setdefault((device_type, kind), {})[language] = {
            _key(platform, entry): entry for platform, entry in _entries(path)
        }

    problems = []
    for (device_type, kind), by_language in groups.items():
        keys = set().union(*[set(entries) for entries in by_language.values()])
        for key in sorted(keys, key=str):
            counts = {}
            for language, entries in by_language.items():
                entry = entries.get(key)
                if entry is None:
                    counts[language] = "missing"
                elif entry.get("options") is None:
                    counts[language] = None
                else:
                    counts[language] = len(entry["options"])
            if len({str(count) for count in counts.values()}) > 1:
                problems.append(f"{device_type}/{kind} {key}: {counts}")
    assert not problems, "preset languages disagree:\n" + "\n".join(problems)
