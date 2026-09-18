"""Tests for rewriting the OTA encryption key in place."""

from __future__ import annotations

from pathlib import Path
import sys
from typing import Any
from unittest.mock import patch

import pytest

from esphome import yaml_edit, yaml_util
from esphome.compiled_config import compiled_config_path
from esphome.config import do_substitution_pass
from esphome.core import CORE, EsphomeError
from esphome.yaml_edit import (
    KeyEdit,
    Snapshot,
    apply_key_edits,
    locate_key_edits,
    old_key_edit,
    restore_key_files,
)

OLD_KEY = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="
OLDER_KEY = "AgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICE="
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
    CORE.raw_config = do_substitution_pass(yaml_util.load_yaml(CORE.config_path), None)
    return CORE.config_path


def _rotate() -> list[KeyEdit]:
    """Locate and apply everything a rotation writes."""
    edits = [*locate_key_edits(OLD_KEY, NEW_KEY), *old_key_edit(OLD_KEY)]
    apply_key_edits(edits)
    return edits


def test_inline_api_key(tmp_path: Path) -> None:
    """A bare ota block inherits the api key, so the api line is the one
    rewritten, quotes and comment kept."""
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(path, 5)]
    assert edits[0].new_line == f'    key: "{NEW_KEY}"  # shared with ota'
    originals = apply_key_edits(edits)
    assert originals == {path: Snapshot(API_YAML, API_YAML.replace(OLD_KEY, NEW_KEY))}
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


def test_secret_api_key_next_to_a_literal_ota_key(tmp_path: Path) -> None:
    """An explicit ota key must match the api key at validation; both the
    secrets line and the literal line change so they still do."""
    yaml_text = SECRET_YAML.replace(
        "      key: !secret device_key\n", f'      key: "{OLD_KEY}"\n'
    )
    _setup(tmp_path, yaml_text, f"device_key: {OLD_KEY}\n")
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert sorted(e.path.name for e in edits) == ["secrets.yaml", "test.yaml"]
    apply_key_edits(edits)
    assert (tmp_path / "secrets.yaml").read_text() == f"device_key: {NEW_KEY}\n"
    assert CORE.config_path.read_text() == yaml_text.replace(OLD_KEY, NEW_KEY)


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
    _setup(tmp_path, yaml_text)
    # Includes are deferred until the substitution pass, as in read_config
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
    _setup(tmp_path, yaml_text)
    with pytest.raises(EsphomeError, match="edit the key by hand"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_refuses_a_secret_defined_elsewhere(tmp_path: Path) -> None:
    """A secret whose value the line regex cannot find, for example a folded
    scalar, is refused rather than guessed at."""
    secrets = f"device_key: >-\n  {OLD_KEY}\n"
    _setup(tmp_path, SECRET_YAML, secrets)
    with pytest.raises(EsphomeError, match="No 'device_key:' line"):
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
    _setup(tmp_path, API_YAML)
    good = tmp_path / "a.yaml"
    (tmp_path / "b.yaml").write_bytes(b"")
    good.write_bytes(b"")
    with (
        patch("esphome.yaml_edit.write_file", side_effect=[EsphomeError("disk"), None]),
        pytest.raises(EsphomeError, match="Could not restore .*b.yaml: disk"),
    ):
        restore_key_files(
            {tmp_path / "b.yaml": Snapshot("x", ""), good: Snapshot("y", "")}
        )


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
    edits = [*locate_key_edits(OLD_KEY, NEW_KEY), *old_key_edit(OLD_KEY)]
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
    _rotate()
    assert path.read_text() == yaml_text.replace(
        f'      key: "{OLD_KEY}"\n',
        f'      key: "{NEW_KEY}"\n      old_key: "{OLD_KEY}"\n',
    )


def test_old_key_rewritten_when_present(tmp_path: Path) -> None:
    """A second rotation replaces the previous old_key in place."""
    yaml_text = f"""esphome:
  name: test

api:
  encryption:
    key: "{OLD_KEY}"

ota:
  - platform: esphome
    encryption:
      old_key: '{OLDER_KEY}'  # from the last rotation
"""
    path = _setup(tmp_path, yaml_text)
    _rotate()
    assert path.read_text() == yaml_text.replace(OLD_KEY, NEW_KEY).replace(
        OLDER_KEY, OLD_KEY
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
    _rotate()
    assert path.read_text() == yaml_text.replace(OLD_KEY, NEW_KEY) + (
        f'    old_key: "{OLD_KEY}"\n'
    )


def test_keeps_windows_line_endings_and_a_bare_last_line(tmp_path: Path) -> None:
    text = API_YAML.replace("\n", "\r\n").rstrip("\r\n")
    path = _setup(tmp_path, text)
    _rotate()
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
    edits = [*locate_key_edits(OLD_KEY, NEW_KEY), *old_key_edit(OLD_KEY)]
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
    _rotate()
    assert path.read_text() == yaml_text.replace(OLD_KEY, "TMP").replace(
        NEW_KEY, OLD_KEY
    ).replace("TMP", NEW_KEY)


def test_key_from_the_data_dir_is_refused(tmp_path: Path) -> None:
    """Remote packages are checked out under .esphome and are not the user's."""
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
    with pytest.raises(EsphomeError, match="not an editable file"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_secret_in_an_include_falls_back_to_the_main_secrets(tmp_path: Path) -> None:
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
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(tmp_path / "secrets.yaml", 0)]
    # A file beside the include that does not parse is skipped by the
    # loader too, so the main one is still the target
    (tmp_path / "sub" / "secrets.yaml").write_bytes(b": :\n")
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(tmp_path / "secrets.yaml", 0)]


def test_split_with_key_first_and_old_key_later(tmp_path: Path) -> None:
    """old_key lands on the entry that already carries it, whichever entry
    comes first; a second old_key would make the merged config inconsistent."""
    yaml_text = f"""esphome:
  name: test

ota:
  - platform: esphome
    encryption:
      key: "{OLD_KEY}"
  - platform: esphome
    encryption:
      old_key: "{OLDER_KEY}"
"""
    path = _setup(tmp_path, yaml_text)
    _rotate()
    assert path.read_text() == yaml_text.replace(OLD_KEY, NEW_KEY).replace(
        OLDER_KEY, OLD_KEY
    )


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


def test_key_from_an_include_tag_is_refused(tmp_path: Path) -> None:
    """`key: !include file` is indirect like a substitution; the value it
    loads is not a line of this yaml."""
    (tmp_path / "key.yaml").write_bytes(f'"{OLD_KEY}"\n'.encode())
    _setup(tmp_path, API_YAML.replace(f'key: "{OLD_KEY}"', "key: !include key.yaml"))
    with pytest.raises(EsphomeError, match="edit the key by hand"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_old_key_from_a_substitution_is_refused(tmp_path: Path) -> None:
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


def test_other_platform_listed_first(tmp_path: Path) -> None:
    yaml_text = f"""esphome:
  name: test

api:
  encryption:
    key: "{OLD_KEY}"

ota:
  - platform: web_server
  - platform: esphome
    encryption:
"""
    path = _setup(tmp_path, yaml_text)
    _rotate()
    assert path.read_text() == yaml_text.replace(OLD_KEY, NEW_KEY) + (
        f'      old_key: "{OLD_KEY}"\n'
    )


def test_block_scalar_key_in_the_config_is_refused(tmp_path: Path) -> None:
    yaml_text = f"""esphome:
  name: test

api:
  encryption:
    key: >-
      {OLD_KEY}

ota:
  - platform: esphome
    encryption:
"""
    _setup(tmp_path, yaml_text)
    with pytest.raises(EsphomeError, match="edit the key by hand"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_comment_after_the_key_line_stays_below_old_key(tmp_path: Path) -> None:
    yaml_text = f"""esphome:
  name: test

mqtt:
  broker: broker.local

ota:
  - platform: esphome
    encryption:
      key: "{OLD_KEY}"
      # rotated on install
    port: 3232
"""
    path = _setup(tmp_path, yaml_text)
    _rotate()
    assert path.read_text() == yaml_text.replace(
        f'      key: "{OLD_KEY}"\n',
        f'      key: "{NEW_KEY}"\n      old_key: "{OLD_KEY}"\n',
    )


def test_quoted_and_prefixed_secret_names(tmp_path: Path) -> None:
    """A quoted name still matches, a longer name that starts the same and a
    nested same-named key do not."""
    secrets = f"""device_key_old: {OLD_KEY}
nested:
  device_key: {OLD_KEY}
"device_key": {OLD_KEY}
"""
    _setup(tmp_path, SECRET_YAML, secrets)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.line, e.new_line) for e in edits] == [(3, f'"device_key": {NEW_KEY}')]


def test_quoted_secret_reference(tmp_path: Path) -> None:
    """`!secret "name"` is valid yaml; the quotes are not part of the name."""
    _setup(
        tmp_path,
        SECRET_YAML.replace("!secret device_key", '!secret "device_key"'),
        f"device_key: {OLD_KEY}\n",
    )
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path.name, e.new_line) for e in edits] == [
        ("secrets.yaml", f"device_key: {NEW_KEY}")
    ]


@pytest.mark.skipif(sys.platform == "win32", reason="posix file modes")
def test_keeps_the_secrets_file_mode(tmp_path: Path) -> None:
    """A 0600 secrets.yaml stays 0600 through the rewrite and the restore."""
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    secrets = tmp_path / "secrets.yaml"
    secrets.chmod(0o600)
    originals = apply_key_edits(locate_key_edits(OLD_KEY, NEW_KEY))
    assert secrets.stat().st_mode & 0o777 == 0o600
    assert NEW_KEY in secrets.read_text()
    restore_key_files(originals)
    assert secrets.stat().st_mode & 0o777 == 0o600
    assert OLD_KEY in secrets.read_text()


def test_symlinked_secrets_file_is_edited_in_place(tmp_path: Path) -> None:
    """The rewrite lands on the link's target, so the link survives and
    other configurations sharing the file see the new key."""
    target = tmp_path / "shared" / "secrets.yaml"
    target.parent.mkdir()
    target.write_bytes(f"device_key: {OLD_KEY}\n".encode())
    (tmp_path / "secrets.yaml").symlink_to(target)
    _setup(tmp_path, SECRET_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [e.path for e in edits] == [target.resolve()]
    apply_key_edits(edits)
    assert (tmp_path / "secrets.yaml").is_symlink()
    assert NEW_KEY in target.read_text()


def test_refuses_a_secrets_file_linked_outside_the_config_dir(
    tmp_path: Path,
) -> None:
    config_dir = tmp_path / "config"
    config_dir.mkdir()
    outside = tmp_path / "secrets.yaml"
    outside.write_bytes(f"device_key: {OLD_KEY}\n".encode())
    (config_dir / "secrets.yaml").symlink_to(outside)
    _setup(config_dir, SECRET_YAML)
    with pytest.raises(EsphomeError, match="not an editable file"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_old_key_goes_into_the_included_block(tmp_path: Path) -> None:
    """`encryption: !include enc.yaml` keeps the key in another file; old_key
    is inserted after that file's key line, not at the same line number of
    the main yaml."""
    (tmp_path / "enc.yaml").write_bytes(f'key: "{OLD_KEY}"\n'.encode())
    _setup(
        tmp_path,
        "esphome:\n  name: test\n\nota:\n  - platform: esphome\n"
        "    encryption: !include enc.yaml\n",
    )
    (edit,) = old_key_edit(OLD_KEY)
    assert (edit.path, edit.line, edit.insert_after) == (
        (tmp_path / "enc.yaml").resolve(),
        0,
        True,
    )
    assert edit.new_line == f'old_key: "{OLD_KEY}"'
    apply_key_edits([*locate_key_edits(OLD_KEY, NEW_KEY), edit])
    assert (tmp_path / "enc.yaml").read_text() == (
        f'key: "{NEW_KEY}"\nold_key: "{OLD_KEY}"\n'
    )


def test_truncated_file_is_a_stale_edit(tmp_path: Path) -> None:
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    path.write_bytes(b"esphome:\n  name: test\n")
    with pytest.raises(EsphomeError, match="changed since it was read"):
        apply_key_edits(edits)


def test_secret_shared_with_other_configs_is_reported(tmp_path: Path) -> None:
    """Other yaml files using the same secret are listed; this config's own
    files, the build data and other secret names are not."""
    (tmp_path / "other.yaml").write_bytes(
        b"api:\n  encryption:\n    key: !secret device_key\n"
    )
    (tmp_path / "quoted.yaml").write_bytes(
        b'ota:\n  - platform: esphome\n    encryption:\n      key: !secret "device_key"\n'
    )
    (tmp_path / "unrelated.yaml").write_bytes(
        b"wifi:\n  password: !secret device_key_old\n"
    )
    build = tmp_path / ".esphome" / "build"
    build.mkdir(parents=True)
    (build / "copy.yaml").write_bytes(b"key: !secret device_key\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\ndevice_key_old: x\n")
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [e.shared_with for e in edits] == [
        [tmp_path.resolve() / "other.yaml", tmp_path.resolve() / "quoted.yaml"]
    ]


def test_secret_key_keeps_the_previous_key_in_secrets(tmp_path: Path) -> None:
    """A key from secrets.yaml never lands in the device yaml as text: the
    ota block gets `old_key: !secret device_key_old` and the secrets file
    the line it points to, right under the rewritten one."""
    _setup(tmp_path, SECRET_YAML, f"wifi: hunter2\ndevice_key: {OLD_KEY}\n")
    edits = [*locate_key_edits(OLD_KEY, NEW_KEY), *old_key_edit(OLD_KEY)]
    apply_key_edits(edits)
    assert CORE.config_path.read_text() == SECRET_YAML.replace(
        "      key: !secret device_key\n",
        "      key: !secret device_key\n      old_key: !secret device_key_old\n",
    )
    assert (tmp_path / "secrets.yaml").read_text() == (
        f'wifi: hunter2\ndevice_key: {NEW_KEY}\ndevice_key_old: "{OLD_KEY}"\n'
    )


def test_secret_old_key_rewritten_in_secrets(tmp_path: Path) -> None:
    """A second rotation replaces the value the `!secret` old_key points to."""
    yaml_text = SECRET_YAML.replace(
        "      key: !secret device_key\n",
        "      key: !secret device_key\n      old_key: !secret device_key_old\n",
    )
    secrets = f"device_key: {OLD_KEY}\ndevice_key_old: '{OLDER_KEY}'  # keep\n"
    _setup(tmp_path, yaml_text, secrets)
    _rotate()
    assert CORE.config_path.read_text() == yaml_text
    assert (tmp_path / "secrets.yaml").read_text() == (
        f"device_key: {NEW_KEY}\ndevice_key_old: '{OLD_KEY}'  # keep\n"
    )


def test_bare_block_with_a_secret_api_key(tmp_path: Path) -> None:
    """The inherited key is a secret too, so old_key points at secrets.yaml."""
    yaml_text = SECRET_YAML.replace(
        "    encryption:\n      key: !secret device_key\n", "    encryption:\n"
    )
    _setup(tmp_path, yaml_text, f"device_key: {OLD_KEY}\n")
    _rotate()
    assert CORE.config_path.read_text() == yaml_text.replace(
        "    encryption:\n", "    encryption:\n      old_key: !secret device_key_old\n"
    )


def test_shared_secret_scan_covers_yml_and_skips_unreadable_files(
    tmp_path: Path,
) -> None:
    (tmp_path / "other.yml").write_bytes(
        b"api:\n  encryption:\n    key: !secret device_key\n"
    )
    (tmp_path / "latin1.yaml").write_bytes(b"caf\xe9: !secret device_key\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [e.shared_with for e in edits] == [[tmp_path.resolve() / "other.yml"]]


def test_literal_key_in_a_shared_include_is_reported(tmp_path: Path) -> None:
    """Another configuration including the file that holds the key is
    listed, like a shared secret."""
    (tmp_path / "common.yaml").write_bytes(
        f'encryption:\n  key: "{OLD_KEY}"\n'.encode()
    )
    (tmp_path / "other.yaml").write_bytes(b"api: !include common.yaml\n")
    _setup(
        tmp_path,
        "esphome:\n  name: test\n\napi: !include common.yaml\n\n"
        "ota:\n  - platform: esphome\n    encryption:\n",
    )
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path.name, e.shared_with) for e in edits] == [
        ("common.yaml", [tmp_path.resolve() / "other.yaml"])
    ]


def test_restore_reports_a_missing_file(tmp_path: Path) -> None:
    _setup(tmp_path, API_YAML)
    with pytest.raises(
        EsphomeError, match="Could not restore .*gone.yaml: Error reading file"
    ):
        restore_key_files({tmp_path / "gone.yaml": Snapshot("x")})


def test_mode_failure_is_reported(tmp_path: Path) -> None:
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    with (
        patch("pathlib.Path.chmod", side_effect=OSError("denied")),
        pytest.raises(EsphomeError, match="Could not keep the mode"),
    ):
        apply_key_edits(edits)
    assert path.read_text() == API_YAML


def test_indented_secrets_root_and_space_before_colon(tmp_path: Path) -> None:
    """A secrets file may indent its whole root mapping, and `key :` is
    valid yaml; the added line follows the root indent."""
    secrets = f"  wifi : hunter2\n  device_key : {OLD_KEY}\n"
    _setup(tmp_path, SECRET_YAML.replace("key: !secret", "key : !secret"), secrets)
    _rotate()
    assert (tmp_path / "secrets.yaml").read_text() == (
        f'  wifi : hunter2\n  device_key : {NEW_KEY}\n  device_key_old: "{OLD_KEY}"\n'
    )
    assert "old_key: !secret device_key_old" in CORE.config_path.read_text()


def test_old_key_refuses_a_flow_style_key(tmp_path: Path) -> None:
    """`encryption: {key: ...}` puts the key on the block's own line."""
    _setup(
        tmp_path,
        "esphome:\n  name: test\n\nota:\n  - platform: esphome\n"
        f'    encryption: {{key: "{OLD_KEY}"}}\n',
    )
    with pytest.raises(EsphomeError, match="does not hold 'key:' as a block line"):
        old_key_edit(OLD_KEY)


def test_restore_reports_a_cache_it_could_not_drop(tmp_path: Path) -> None:
    path = _setup(tmp_path, API_YAML)
    with (
        patch(
            "esphome.compiled_config.invalidate_compiled_config",
            side_effect=EsphomeError("busy"),
        ),
        pytest.raises(EsphomeError, match="Could not restore busy"),
    ):
        restore_key_files({path: Snapshot(API_YAML)})


def test_own_includes_and_similar_names_are_not_shared_users(tmp_path: Path) -> None:
    """A file this configuration reaches through another include is its own,
    and `wifi_common.yaml` does not mention `common.yaml`."""
    (tmp_path / "common.yaml").write_bytes(f'key: "{OLD_KEY}"\n'.encode())
    (tmp_path / "base.yaml").write_bytes(b"encryption: !include common.yaml\n")
    (tmp_path / "other.yaml").write_bytes(b"api: !include base.yaml\n")
    (tmp_path / "lookalike.yaml").write_bytes(b"wifi: !include wifi_common.yaml\n")
    _setup(
        tmp_path,
        "esphome:\n  name: test\n\napi: !include base.yaml\n\n"
        "ota:\n  - platform: esphome\n    encryption:\n",
    )
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path.name, e.shared_with) for e in edits] == [("common.yaml", [])]


def test_existing_old_secret_used_elsewhere_is_not_overwritten(
    tmp_path: Path,
) -> None:
    """A `<name>_old` line belongs to whoever references it, this
    configuration included since it has no old_key; a leftover nobody uses
    is reused."""
    secrets = f"device_key: {OLD_KEY}\ndevice_key_old: hunter2\n"
    _setup(tmp_path, SECRET_YAML, secrets)
    (edit, kept) = old_key_edit(OLD_KEY)
    assert (kept.new_line, kept.shared_with) == (f"device_key_old: {OLD_KEY}", [])
    (tmp_path / "other.yaml").write_bytes(
        b"wifi:\n  password: !secret device_key_old\n"
    )
    with pytest.raises(
        EsphomeError, match="'device_key_old:' in .* is used by .*other.yaml"
    ):
        old_key_edit(OLD_KEY)
    (tmp_path / "other.yaml").unlink()
    _setup(
        tmp_path, SECRET_YAML + "wifi:\n  password: !secret device_key_old\n", secrets
    )
    with pytest.raises(
        EsphomeError, match="'device_key_old:' in .* is used by .*test.yaml"
    ):
        old_key_edit(OLD_KEY)


def test_key_secret_with_a_folded_value_gets_no_old_line(tmp_path: Path) -> None:
    """The `<name>_old` line goes under the key's own line, which must be plain."""
    _setup(tmp_path, SECRET_YAML, f"device_key: >-\n  {OLD_KEY}\n")
    with pytest.raises(EsphomeError, match="No plain 'device_key:' line"):
        old_key_edit(OLD_KEY)


def test_old_secret_collision_check_refuses_an_unreadable_file(
    tmp_path: Path,
) -> None:
    """A file the scan cannot read might use the line, so it is not reused."""
    (tmp_path / "latin1.yaml").write_bytes(b"caf\xe9: 1\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\ndevice_key_old: hunter2\n")
    with pytest.raises(EsphomeError, match="Could not read every configuration"):
        old_key_edit(OLD_KEY)


def test_old_key_secret_with_a_folded_value_is_refused(tmp_path: Path) -> None:
    yaml_text = SECRET_YAML.replace(
        "      key: !secret device_key\n",
        "      key: !secret device_key\n      old_key: !secret device_key_old\n",
    )
    _setup(
        tmp_path, yaml_text, f"device_key: {OLD_KEY}\ndevice_key_old: >-\n  {NEW_KEY}\n"
    )
    with pytest.raises(EsphomeError, match="No plain 'device_key_old:' line"):
        old_key_edit(OLD_KEY)


def test_file_shortened_after_the_load_is_reported(tmp_path: Path) -> None:
    path = _setup(tmp_path, API_YAML)
    path.write_bytes(b"esphome:\n  name: test\n")
    with pytest.raises(EsphomeError, match="changed since it was read"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_restore_leaves_a_file_the_user_changed_alone(tmp_path: Path) -> None:
    """An edit made during the compile is not overwritten by the rollback;
    the file is reported so the previous key can be put back by hand."""
    path = _setup(tmp_path, API_YAML)
    originals = apply_key_edits(locate_key_edits(OLD_KEY, NEW_KEY))
    edited = path.read_text() + "logger:\n"
    path.write_bytes(edited.encode())
    with pytest.raises(EsphomeError, match="changed since it was written, left as is"):
        restore_key_files(originals)
    assert path.read_text() == edited


def test_rolls_back_when_the_cache_cannot_be_dropped(tmp_path: Path) -> None:
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    with (
        patch(
            "esphome.compiled_config.invalidate_compiled_config",
            side_effect=[EsphomeError("busy"), None],
        ),
        pytest.raises(EsphomeError, match="busy"),
    ):
        apply_key_edits(edits)
    assert path.read_text() == API_YAML


def test_rolls_back_on_an_interrupt_during_the_reload(tmp_path: Path) -> None:
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    with (
        patch("esphome.yaml_util.load_yaml", side_effect=KeyboardInterrupt),
        pytest.raises(KeyboardInterrupt),
    ):
        apply_key_edits(edits)
    assert path.read_text() == API_YAML


def test_apply_reports_a_rollback_that_also_failed(tmp_path: Path) -> None:
    """The rewrite lands, the reload fails, and the rollback write fails too."""
    from esphome.yaml_edit import write_file

    _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    writes: list[int] = []

    def write_then_fail(*args: Any, **kwargs: Any) -> None:
        writes.append(1)
        if len(writes) > 1:
            raise EsphomeError("disk")
        write_file(*args, **kwargs)

    with (
        patch("esphome.yaml_util.load_yaml", side_effect=EsphomeError("broken")),
        patch("esphome.yaml_edit.write_file", side_effect=write_then_fail),
        pytest.raises(EsphomeError, match="broken; Could not restore .*disk"),
    ):
        apply_key_edits(edits)


def test_two_same_port_entries_from_a_package_split(tmp_path: Path) -> None:
    """A bare block from a package and a keyed block from the device: the
    keyed one is rewritten and gets old_key, the bare one is left alone."""
    yaml_text = f"""esphome:
  name: test

ota:
  - platform: esphome
    encryption:
  - platform: esphome
    encryption:
      key: "{OLD_KEY}"
"""
    path = _setup(tmp_path, yaml_text)
    _rotate()
    assert path.read_text() == yaml_text.replace(
        f'      key: "{OLD_KEY}"\n',
        f'      key: "{NEW_KEY}"\n      old_key: "{OLD_KEY}"\n',
    )


def test_flow_style_bare_block_is_refused(tmp_path: Path) -> None:
    yaml_text = f"""esphome:
  name: test

api:
  encryption:
    key: "{OLD_KEY}"

ota:
  - platform: esphome
    encryption: {{}}
"""
    _setup(tmp_path, yaml_text)
    with pytest.raises(
        EsphomeError, match="does not hold 'encryption:' as a block line"
    ):
        old_key_edit(OLD_KEY)
