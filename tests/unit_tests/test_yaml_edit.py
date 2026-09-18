"""Tests for rewriting the OTA encryption key in place."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome import yaml_util
from esphome.compiled_config import compiled_config_path
from esphome.core import CORE, EsphomeError
from esphome.yaml_edit import apply_key_edits, locate_key_edits, restore_key_files

OLD_KEY = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="
NEW_KEY = "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA="

API_YAML = f"""esphome:
  name: test

api:
  encryption:
    key: "{OLD_KEY}"  # shared with ota

ota:
  - platform: esphome
    encryption:
"""

SECRET_YAML = """esphome:
  name: test

api:
  encryption:
    key: !secret device_key

ota:
  - platform: esphome
    encryption:
      key: !secret device_key
"""


def _setup(tmp_path: Path, yaml_text: str, secrets: str | None = None) -> Path:
    """Write the yaml (and secrets.yaml), point CORE at it and load the raw
    config the way read_config does, so key nodes carry their source range."""
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    CORE.config_path.write_text(yaml_text, encoding="utf-8")
    if secrets is not None:
        (tmp_path / "secrets.yaml").write_text(secrets, encoding="utf-8")
    CORE.raw_config = yaml_util.load_yaml(CORE.config_path)
    return CORE.config_path


def test_inline_api_key(tmp_path: Path) -> None:
    """A bare ota block inherits the api key, so the api line is the one
    rewritten, quotes and comment kept."""
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(path, 5)]
    assert edits[0].new_line == f'    key: "{NEW_KEY}"  # shared with ota'
    originals = apply_key_edits(edits)
    assert originals == {path: API_YAML}
    assert path.read_text() == API_YAML.replace(OLD_KEY, NEW_KEY)
    restore_key_files(originals)
    assert path.read_text() == API_YAML


def test_shared_secret(tmp_path: Path) -> None:
    """Both blocks pointing at one secret give one edit in secrets.yaml, and
    only that line changes: another secret with the same value stays."""
    secrets = f"""wifi_password: hunter2
device_key: '{OLD_KEY}'  # keep
other_key: "{OLD_KEY}"
"""
    _setup(tmp_path, SECRET_YAML, secrets)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(tmp_path / "secrets.yaml", 1)]
    apply_key_edits(edits)
    assert (tmp_path / "secrets.yaml").read_text() == secrets.replace(
        f"device_key: '{OLD_KEY}'", f"device_key: '{NEW_KEY}'"
    )
    assert CORE.config_path.read_text() == SECRET_YAML


def test_explicit_ota_key_next_to_api(tmp_path: Path) -> None:
    yaml_text = f"""esphome:
  name: test

api:
  encryption:
    key: {OLD_KEY}

ota:
  - platform: esphome
    encryption:
      key: {OLD_KEY}
"""
    path = _setup(tmp_path, yaml_text)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert sorted(e.line for e in edits) == [5, 10]
    apply_key_edits(edits)
    assert path.read_text() == yaml_text.replace(OLD_KEY, NEW_KEY)


def test_key_in_an_included_file(tmp_path: Path) -> None:
    """A key that lives in an included file is rewritten there."""
    (tmp_path / "api.yaml").write_text(
        f"""encryption:
  key: "{OLD_KEY}"
""",
        encoding="utf-8",
    )
    yaml_text = """esphome:
  name: test

api: !include api.yaml

ota:
  - platform: esphome
    encryption:
"""
    from esphome.config import do_substitution_pass

    _setup(tmp_path, yaml_text)
    # Includes are deferred until the substitution pass, as in read_config
    CORE.raw_config = do_substitution_pass(CORE.raw_config, None)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(tmp_path / "api.yaml", 1)]
    apply_key_edits(edits)
    assert NEW_KEY in (tmp_path / "api.yaml").read_text()
    assert CORE.config_path.read_text() == yaml_text


def test_own_ota_key_without_api(tmp_path: Path) -> None:
    yaml_text = f"""esphome:
  name: test

mqtt:
  broker: broker.local

ota:
  - platform: esphome
    encryption:
      key: "{OLD_KEY}"
"""
    _setup(tmp_path, yaml_text)
    assert [e.line for e in locate_key_edits(OLD_KEY, NEW_KEY)] == [9]


@pytest.mark.parametrize(
    "yaml_text",
    [
        f"""esphome:
  name: test
substitutions:
  k: "{OLD_KEY}"
api:
  encryption:
    key: ${{k}}
ota:
  - platform: esphome
    encryption:
""",
        f"""esphome:
  name: test
api:
  encryption: {{key: "{OLD_KEY}"}}
ota:
  - platform: esphome
    encryption:
""",
    ],
    ids=["substitution", "flow_mapping"],
)
def test_refuses_what_it_cannot_rewrite(tmp_path: Path, yaml_text: str) -> None:
    from esphome.config import do_substitution_pass

    _setup(tmp_path, yaml_text)
    CORE.raw_config = do_substitution_pass(CORE.raw_config, None)
    with pytest.raises(EsphomeError, match="edit the key by hand"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_refuses_a_secret_defined_elsewhere(tmp_path: Path) -> None:
    """A secret whose value the line regex cannot find, for example a folded
    scalar, is refused rather than guessed at."""
    secrets = f"device_key: >-\n  {OLD_KEY}\n"
    _setup(tmp_path, SECRET_YAML, secrets)
    with pytest.raises(EsphomeError, match="Expected exactly one"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_rolls_back_a_broken_rewrite(tmp_path: Path) -> None:
    """A rewrite the parser does not agree with is undone before anything
    else happens."""
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    edits[0].new_line = "    key: [unterminated"
    with pytest.raises(EsphomeError, match="no longer loads"):
        apply_key_edits(edits)
    assert path.read_text() == API_YAML


def test_clears_the_validated_cache(tmp_path: Path) -> None:
    _setup(tmp_path, API_YAML)
    cache = compiled_config_path(CORE.config_filename)
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text("{}")
    originals = apply_key_edits(locate_key_edits(OLD_KEY, NEW_KEY))
    assert not cache.exists()
    cache.write_text("{}")
    restore_key_files(originals)
    assert not cache.exists()
