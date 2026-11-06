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
        # The C++ option map keys on uint8_t, so a larger value would be truncated.
        assert all(0 <= value <= 0xFF for value in values), (
            f"{path.name}: option values outside 0..255 for {entry.get('name')!r}"
        )


@pytest.mark.parametrize(
    ("device_type", "kind", "language", "path"),
    ALL_FILES,
    ids=[f"{d}/{k}_{lang}" for d, k, lang, _ in ALL_FILES],
)
def test_number_ranges(device_type, kind, language, path) -> None:
    """Every number preset offers a non-empty range, as the schema requires."""
    for platform, entry in _entries(path):
        if platform != "number":
            continue
        assert entry["min_value"] < entry["max_value"], (
            f"{path.name}: min_value {entry['min_value']} is not below max_value "
            f"{entry['max_value']} for {entry.get('name')!r}"
        )


@pytest.mark.parametrize(
    ("device_type", "kind", "language", "path"),
    ALL_FILES,
    ids=[f"{d}/{k}_{lang}" for d, k, lang, _ in ALL_FILES],
)
def test_names_avoid_url_reserved_characters(device_type, kind, language, path) -> None:
    """'/' is a URL path separator: ESPHome warns today and rejects the name in 2027.7.0."""
    for _, entry in _entries(path):
        name = entry.get("name") or ""
        assert "/" not in name, f"{path.name}: {name!r} contains '/'"


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


def _by_address(path: Path, platform: str) -> dict[tuple, dict]:
    """Index one platform's entries of a preset file by function group, number and datapoint."""
    return {
        (
            entry.get("function_group"),
            entry.get("function_number"),
            entry.get("datapoint"),
        ): entry
        for platform_name, entry in _entries(path)
        if platform_name == platform
    }


def _linked_entries(input_platform: str, read_platform: str):
    """Yield (label, input entry, read entry) for every input with a read entity on its address."""
    for device_type in sorted({device_type for device_type, _, _, _ in ALL_FILES}):
        for language in LANGS:
            inputs = PRESETS / device_type / f"inputs_{language}.yaml"
            reads = PRESETS / device_type / f"sensors_{language}.yaml"
            if not inputs.exists() or not reads.exists():
                continue
            read_entries = _by_address(reads, read_platform)
            for address, entry in _by_address(inputs, input_platform).items():
                if (read := read_entries.get(address)) is not None:
                    label = f"{device_type}/{language} {address[0]}/{address[1]}/{address[2]}"
                    yield label, entry, read


def test_selects_match_their_text_sensor() -> None:
    """link_inputs_ forwards the text_sensor state to the select, so the option lists must agree."""
    pairs = list(_linked_entries("select", "text_sensor"))
    assert pairs, "no select/text_sensor pair found in the corpus"
    problems = [
        f"{label}: '{select.get('name')}' does not match the text_sensor options/values"
        for label, select, text_sensor in pairs
        if (select.get("options"), select.get("values"))
        != (text_sensor.get("options"), text_sensor.get("values"))
    ]
    assert not problems, "select and text_sensor disagree:\n" + "\n".join(problems)


def test_numbers_scale_like_their_sensor() -> None:
    """link_inputs_ divides the sensor state by the number's multiplier, so decimal must match."""
    pairs = list(_linked_entries("number", "sensor"))
    assert pairs, "no number/sensor pair found in the corpus"
    problems = []
    for label, number, sensor in pairs:
        expected = 10 ** -number.get("decimal", 0)
        multiply = None
        for filter_config in sensor.get("filters") or []:
            if isinstance(filter_config, dict) and "multiply" in filter_config:
                multiply = float(filter_config["multiply"])
        actual = 1.0 if multiply is None else multiply
        if abs(actual - expected) > 1e-9:
            problems.append(
                f"{label}: '{number.get('name')}' with decimal "
                f"{number.get('decimal', 0)} expects multiply {expected}, sensor has {actual}"
            )
    assert not problems, "number and sensor disagree:\n" + "\n".join(problems)
