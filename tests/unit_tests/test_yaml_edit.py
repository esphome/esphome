"""Tests for rewriting the OTA encryption key in place."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome import yaml_edit, yaml_util
from esphome.compiled_config import compiled_config_path
from esphome.core import CORE, EsphomeError
from esphome.yaml_edit import (
    apply_key_edits,
    locate_key_edits,
    old_key_edit,
    restore_key_files,
)

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
    # Bytes, so Windows does not turn the newlines into CRLF on the way in
    CORE.config_path.write_bytes(yaml_text.encode())
    if secrets is not None:
        (tmp_path / "secrets.yaml").write_bytes(secrets.encode())
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


def test_refuses_a_line_that_changed_since_it_was_located(tmp_path: Path) -> None:
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    path.write_bytes(API_YAML.replace("  # shared with ota", "").encode())
    with pytest.raises(EsphomeError, match="changed since it was read"):
        apply_key_edits(edits)


def test_restore_reports_every_file_it_could_not_write(tmp_path: Path) -> None:
    from unittest.mock import patch

    _setup(tmp_path, API_YAML)
    good = tmp_path / "a.yaml"
    with (
        patch("esphome.yaml_edit.write_file", side_effect=[EsphomeError("disk"), None]),
        pytest.raises(EsphomeError, match="Could not restore .*b.yaml: disk"),
    ):
        restore_key_files({tmp_path / "b.yaml": "x", good: "y"})


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


def test_old_key_added_to_a_bare_block(tmp_path: Path) -> None:
    """A bare block gets old_key one level in; the api line is rewritten."""
    path = _setup(tmp_path, API_YAML)
    edits = [*locate_key_edits(OLD_KEY, NEW_KEY), old_key_edit(OLD_KEY)]
    apply_key_edits(edits)
    assert path.read_text() == (
        API_YAML.replace(OLD_KEY, NEW_KEY) + f'      old_key: "{OLD_KEY}"\n'
    )


def test_old_key_added_after_an_explicit_key(tmp_path: Path) -> None:
    yaml_text = f"""esphome:
  name: test

mqtt:
  broker: broker.local

ota:
  - platform: esphome
    encryption:
      key: "{OLD_KEY}"
    port: 3232
"""
    path = _setup(tmp_path, yaml_text)
    apply_key_edits([*locate_key_edits(OLD_KEY, NEW_KEY), old_key_edit(OLD_KEY)])
    assert path.read_text() == yaml_text.replace(
        f'      key: "{OLD_KEY}"\n',
        f'      key: "{NEW_KEY}"\n      old_key: "{OLD_KEY}"\n',
    )


def test_old_key_rewritten_when_present(tmp_path: Path) -> None:
    """A second rotation replaces the previous old_key in place."""
    older = "AgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICE="
    yaml_text = f"""esphome:
  name: test

api:
  encryption:
    key: "{OLD_KEY}"

ota:
  - platform: esphome
    encryption:
      old_key: '{older}'  # from the last rotation
"""
    path = _setup(tmp_path, yaml_text)
    apply_key_edits([*locate_key_edits(OLD_KEY, NEW_KEY), old_key_edit(OLD_KEY)])
    assert path.read_text() == yaml_text.replace(OLD_KEY, NEW_KEY).replace(
        older, OLD_KEY
    )


def test_ota_written_as_a_mapping(tmp_path: Path) -> None:
    """A single-platform ota: written without the list dash is still found."""
    yaml_text = f"""esphome:
  name: test

ota:
  platform: esphome
  encryption:
    key: "{OLD_KEY}"
"""
    path = _setup(tmp_path, yaml_text)
    apply_key_edits([*locate_key_edits(OLD_KEY, NEW_KEY), old_key_edit(OLD_KEY)])
    assert path.read_text() == yaml_text.replace(OLD_KEY, NEW_KEY) + (
        f'    old_key: "{OLD_KEY}"\n'
    )


def test_keeps_windows_line_endings_and_a_bare_last_line(tmp_path: Path) -> None:
    text = API_YAML.replace("\n", "\r\n").rstrip("\r\n")
    path = _setup(tmp_path, text)
    apply_key_edits([*locate_key_edits(OLD_KEY, NEW_KEY), old_key_edit(OLD_KEY)])
    assert (
        path.read_bytes()
        == (text.replace(OLD_KEY, NEW_KEY) + f'\r\n      old_key: "{OLD_KEY}"').encode()
    )


def test_unreadable_file(tmp_path: Path) -> None:
    with pytest.raises(EsphomeError, match="Error reading file"):
        yaml_edit._read_text(tmp_path)


def test_key_missing_from_the_yaml(tmp_path: Path) -> None:
    _setup(tmp_path, API_YAML)
    with pytest.raises(EsphomeError, match="was not found"):
        locate_key_edits(NEW_KEY, OLD_KEY)


def test_key_without_a_source_location(tmp_path: Path) -> None:
    """A raw config built in code has no ranges to edit from."""
    _setup(tmp_path, API_YAML)
    CORE.raw_config = {"api": {"encryption": {"key": OLD_KEY}}}
    with pytest.raises(EsphomeError, match="not found on a line"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_inherited_key_in_a_bare_block_is_not_a_line(tmp_path: Path) -> None:
    """Final validate writes the api key into a bare ota block; that entry
    has no line to rewrite and the api line is the only edit."""
    path = _setup(tmp_path, API_YAML)
    CORE.raw_config["ota"][0]["encryption"] = (
        CORE.raw_config["ota"][0]["encryption"] or {}
    )
    CORE.raw_config["ota"][0]["encryption"]["key"] = OLD_KEY
    edits = [*locate_key_edits(OLD_KEY, NEW_KEY), old_key_edit(OLD_KEY)]
    assert [(e.line, e.insert_after) for e in edits] == [(5, False), (9, True)]
    apply_key_edits(edits)
    assert path.read_text() == (
        API_YAML.replace(OLD_KEY, NEW_KEY) + f'      old_key: "{OLD_KEY}"\n'
    )


def test_values_replaced_by_validation_still_locate(tmp_path: Path) -> None:
    """Validation may swap a value for a plain string; the mapping key keeps
    the range the edit needs."""
    yaml_text = API_YAML.replace(
        "    encryption:\n", f'    encryption:\n      old_key: "{NEW_KEY}"\n', 1
    )
    path = _setup(tmp_path, yaml_text)
    api = CORE.raw_config["api"]["encryption"]
    api["key"] = str(api["key"])
    ota = CORE.raw_config["ota"][0]["encryption"]
    ota["old_key"] = str(ota["old_key"])
    apply_key_edits([*locate_key_edits(OLD_KEY, NEW_KEY), old_key_edit(OLD_KEY)])
    assert path.read_text() == yaml_text.replace(OLD_KEY, "TMP").replace(
        NEW_KEY, OLD_KEY
    ).replace("TMP", NEW_KEY)


def test_key_from_the_data_dir_is_refused(tmp_path: Path) -> None:
    """Remote packages are checked out under .esphome and are not the user's."""
    from esphome.config import do_substitution_pass

    (tmp_path / ".esphome").mkdir()
    (tmp_path / ".esphome" / "api.yaml").write_text(
        f'encryption:\n  key: "{OLD_KEY}"\n', encoding="utf-8"
    )
    _setup(
        tmp_path,
        """esphome:
  name: test

api: !include .esphome/api.yaml

ota:
  - platform: esphome
    encryption:
""",
    )
    CORE.raw_config = do_substitution_pass(CORE.raw_config, None)
    with pytest.raises(EsphomeError, match="not an editable file"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_secret_in_an_include_falls_back_to_the_main_secrets(tmp_path: Path) -> None:
    from esphome.config import do_substitution_pass

    (tmp_path / "sub").mkdir()
    (tmp_path / "sub" / "api.yaml").write_text(
        "encryption:\n  key: !secret device_key\n", encoding="utf-8"
    )
    _setup(
        tmp_path,
        """esphome:
  name: test

api: !include sub/api.yaml

ota:
  - platform: esphome
    encryption:
""",
        f"device_key: {OLD_KEY}\n",
    )
    CORE.raw_config = do_substitution_pass(CORE.raw_config, None)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(tmp_path / "secrets.yaml", 0)]


def test_old_key_needs_an_encryption_block(tmp_path: Path) -> None:
    _setup(
        tmp_path,
        f"""esphome:
  name: test

api:
  encryption:
    key: "{OLD_KEY}"

ota:
  - platform: esphome
""",
    )
    with pytest.raises(EsphomeError, match="no 'encryption:' block"):
        old_key_edit(OLD_KEY)


def test_old_key_from_a_substitution_is_refused(tmp_path: Path) -> None:
    from esphome.config import do_substitution_pass

    _setup(
        tmp_path,
        f"""esphome:
  name: test
substitutions:
  prev: "{NEW_KEY}"
api:
  encryption:
    key: "{OLD_KEY}"
ota:
  - platform: esphome
    encryption:
      old_key: ${{prev}}
""",
    )
    CORE.raw_config = do_substitution_pass(CORE.raw_config, None)
    with pytest.raises(EsphomeError, match="edit it by hand"):
        old_key_edit(OLD_KEY)


def test_old_key_on_a_block_without_source(tmp_path: Path) -> None:
    """An ota block built in code has no line to add old_key after."""
    _setup(tmp_path, API_YAML)
    CORE.raw_config = {
        "api": {"encryption": {"key": OLD_KEY}},
        "ota": [{"platform": "esphome", "encryption": {"old_key": NEW_KEY}}],
    }
    with pytest.raises(EsphomeError, match="was not read from a file"):
        old_key_edit(OLD_KEY)
