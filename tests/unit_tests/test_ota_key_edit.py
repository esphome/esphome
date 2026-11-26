"""Tests for rewriting the OTA encryption key in place."""

from __future__ import annotations

from pathlib import Path
import re
import sys
from unittest.mock import patch

import pytest

from esphome import yaml_util
from esphome.core import CORE, EsphomeError
from esphome.ota_key_edit import KeyEdit, locate_key_edits, old_key_edit, rotation_edits
from esphome.yaml_edit import Snapshot, apply_line_edits, restore_files
from tests.unit_tests import test_yaml_edit

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
    """Write secrets.yaml, then load the yaml as ``test_yaml_edit._setup`` does."""
    if secrets is not None:
        (tmp_path / "secrets.yaml").write_bytes(secrets.encode())
    return test_yaml_edit._setup(tmp_path, yaml_text)


def _device(api_include: str) -> str:
    """A device whose api block is included and whose ota block takes its key."""
    return (
        f"esphome:\n  name: test\n\napi: !include {api_include}\n\n"
        "ota:\n  - platform: esphome\n    encryption:\n"
    )


def _refused(match: str) -> None:
    with pytest.raises(EsphomeError, match=match):
        rotation_edits(OLD_KEY, NEW_KEY)


def _rotate() -> list[KeyEdit]:
    edits = rotation_edits(OLD_KEY, NEW_KEY)
    apply_line_edits(edits)
    return edits


def test_inline_api_key(tmp_path: Path) -> None:
    """A bare ota block inherits the api key, so the api line is the one
    rewritten, quotes and comment kept."""
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(path, 5)]
    assert edits[0].new_line == f'    key: "{NEW_KEY}"  # shared with ota'
    originals = apply_line_edits(edits)
    assert originals == {
        path: Snapshot(
            API_YAML, API_YAML.replace(OLD_KEY, NEW_KEY), originals[path].mode
        )
    }
    assert path.read_text() == API_YAML.replace(OLD_KEY, NEW_KEY)
    restore_files(originals)
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
    apply_line_edits(edits)
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
    apply_line_edits(edits)
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
    apply_line_edits(edits)
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
    apply_line_edits(edits)
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
    with pytest.raises(
        EsphomeError, match="No plain 'device_key:' line with the current key"
    ):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_refuses_a_line_that_changed_since_it_was_located(tmp_path: Path) -> None:
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    path.write_bytes(API_YAML.replace("  # shared with ota", "").encode())
    with pytest.raises(EsphomeError, match="changed since it was read"):
        apply_line_edits(edits)


def test_old_key_added_to_a_bare_block(tmp_path: Path) -> None:
    """A bare block gets old_key one level in; the api line is rewritten."""
    path = _setup(tmp_path, API_YAML)
    edits = rotation_edits(OLD_KEY, NEW_KEY)
    apply_line_edits(edits)
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
    edits = rotation_edits(OLD_KEY, NEW_KEY)
    assert [(e.line, e.insert_after) for e in edits] == [(5, False), (9, True)]
    apply_line_edits(edits)
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
    yaml_text, secrets = _device("sub/api.yaml"), f"device_key: {OLD_KEY}\n"
    _setup(tmp_path, yaml_text, secrets)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(tmp_path / "secrets.yaml", 0)]
    # A file beside the include that does not parse is skipped by the
    # loader too, so the main one is still the target
    (tmp_path / "sub" / "secrets.yaml").write_bytes(b"a: [\n")
    _setup(tmp_path, yaml_text, secrets)  # the secrets file is found once per run
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(tmp_path / "secrets.yaml", 0)]
    # One beside the include that loads is the loader's first choice
    (tmp_path / "sub" / "secrets.yaml").write_bytes(f"device_key: {OLD_KEY}\n".encode())
    _setup(tmp_path, yaml_text, secrets)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(tmp_path / "sub" / "secrets.yaml", 0)]


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
    with pytest.raises(EsphomeError, match="edit 'old_key' by hand"):
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
    originals = apply_line_edits(locate_key_edits(OLD_KEY, NEW_KEY))
    assert secrets.stat().st_mode & 0o777 == 0o600
    assert NEW_KEY in secrets.read_text()
    restore_files(originals)
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
    apply_line_edits(edits)
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
    apply_line_edits([*locate_key_edits(OLD_KEY, NEW_KEY), edit])
    assert (tmp_path / "enc.yaml").read_text() == (
        f'key: "{NEW_KEY}"\nold_key: "{OLD_KEY}"\n'
    )


def test_truncated_file_is_a_stale_edit(tmp_path: Path) -> None:
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    path.write_bytes(b"esphome:\n  name: test\n")
    with pytest.raises(EsphomeError, match="changed since it was read"):
        apply_line_edits(edits)


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
        b"wifi:\n  password: !secret device_key_other\n"
    )
    build = tmp_path / ".esphome" / "build"
    build.mkdir(parents=True)
    (build / "copy.yaml").write_bytes(b"key: !secret device_key\n")
    (tmp_path / "packages").mkdir()
    (tmp_path / "packages" / "base.yaml").write_bytes(b"key: !secret device_key\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\ndevice_key_old: x\n")
    _refused(
        "^secret 'device_key' is also used by other.yaml, packages/base.yaml, "
        "quoted.yaml; only a key unique to this device can be rotated, change it "
        "by hand$"
    )


def test_secret_key_keeps_the_previous_key_in_secrets(tmp_path: Path) -> None:
    """A key from secrets.yaml never lands in the device yaml as text: the
    ota block gets `old_key: !secret device_key_old` and the secrets file
    the line it points to, right under the rewritten one."""
    _setup(tmp_path, SECRET_YAML, f"wifi: hunter2\ndevice_key: {OLD_KEY}\n")
    edits = rotation_edits(OLD_KEY, NEW_KEY)
    apply_line_edits(edits)
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


def test_secret_old_line_that_is_not_plain_is_refused(tmp_path: Path) -> None:
    """A second `<secret>_old:` key would stop secrets.yaml from loading."""
    secrets = f"device_key: {OLD_KEY}\ndevice_key_old: >-\n  {OLDER_KEY}\n"
    _setup(tmp_path, SECRET_YAML, secrets)
    _refused(r"^No plain 'device_key_old:' line in .*secrets\.yaml; edit it by hand$")


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
    (tmp_path / "notes.txt").write_bytes(b"key: !secret device_key\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    _refused("^secret 'device_key' is also used by other.yml;")


def test_shared_include_with_a_hash_in_its_quoted_name_is_reported(
    tmp_path: Path,
) -> None:
    """A `#` inside a quoted include path is part of the name, not a comment."""
    (tmp_path / "shared # ota.yaml").write_bytes(
        f'encryption:\n  key: "{OLD_KEY}"\n'.encode()
    )
    (tmp_path / "other.yaml").write_bytes(
        b'api: !include "shared # ota.yaml"  # the device next door\n'
    )
    _setup(
        tmp_path,
        _device("'shared # ota.yaml'"),
    )
    _refused("^shared # ota.yaml is also used by other.yaml;")


def test_literal_key_in_a_shared_include_is_reported(tmp_path: Path) -> None:
    """Another configuration including the file that holds the key is
    listed, like a shared secret."""
    (tmp_path / "common.yaml").write_bytes(
        f'encryption:\n  key: "{OLD_KEY}"\n'.encode()
    )
    (tmp_path / "other.yaml").write_bytes(b"api: !include common.yaml\n")
    _setup(
        tmp_path,
        _device("common.yaml"),
    )
    _refused("^common.yaml is also used by other.yaml;")


def test_mode_failure_is_reported(tmp_path: Path) -> None:
    path = _setup(tmp_path, API_YAML)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    with (
        patch("pathlib.Path.chmod", side_effect=OSError("denied")),
        pytest.raises(
            EsphomeError, match="was written but could not get its mode back"
        ),
    ):
        apply_line_edits(edits)
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


def test_own_includes_and_similar_names_are_not_shared_users(tmp_path: Path) -> None:
    """A file this configuration reaches through another include is its own
    but still a path for other devices, and `wifi_common.yaml` does not
    mention `common.yaml`."""
    (tmp_path / "common.yaml").write_bytes(f'key: "{OLD_KEY}"\n'.encode())
    (tmp_path / "base.yaml").write_bytes(b"encryption: !include common.yaml\n")
    (tmp_path / "other.yaml").write_bytes(b"api: !include base.yaml\n")
    (tmp_path / "lookalike.yaml").write_bytes(b"wifi: !include wifi_common.yaml\n")
    _setup(
        tmp_path,
        _device("base.yaml"),
    )
    # other.yaml reaches common.yaml through this configuration's own
    # base.yaml, so it is a sharer; base.yaml itself and the lookalike are not
    _refused("^common.yaml is also used by other.yaml;")


def test_existing_old_secret_used_elsewhere_is_not_overwritten(
    tmp_path: Path,
) -> None:
    """A `<name>_old` line belongs to whoever references it, this
    configuration included since it has no old_key; a leftover nobody uses
    is reused."""
    secrets = f"device_key: {OLD_KEY}\ndevice_key_old: hunter2\n"
    _setup(tmp_path, SECRET_YAML, secrets)
    (edit, kept) = old_key_edit(OLD_KEY)
    assert kept.new_line == f"device_key_old: {OLD_KEY}"
    (tmp_path / "other.yaml").write_bytes(
        b"wifi:\n  password: !secret device_key_old\n"
    )
    _setup(tmp_path, SECRET_YAML, secrets)  # the scan runs once per run
    with pytest.raises(
        EsphomeError, match="^secret 'device_key_old' is also used by other.yaml;"
    ):
        rotation_edits(OLD_KEY, NEW_KEY)
    (tmp_path / "other.yaml").unlink()
    _setup(
        tmp_path, SECRET_YAML + "wifi:\n  password: !secret device_key_old\n", secrets
    )
    _refused("^secret 'device_key_old' is also used by test.yaml:13;")


def test_key_secret_with_a_folded_value_gets_no_old_line(tmp_path: Path) -> None:
    """The `<name>_old` line goes under the key's own line, which must be plain."""
    _setup(tmp_path, SECRET_YAML, f"device_key: >-\n  {OLD_KEY}\n")
    with pytest.raises(EsphomeError, match="No plain 'device_key:' line"):
        old_key_edit(OLD_KEY)


def test_old_secret_collision_check_refuses_an_unreadable_file(
    tmp_path: Path,
) -> None:
    """A file the scan cannot read might use the line, so nothing is rotated."""
    (tmp_path / "latin1.yaml").write_bytes(b"caf\xe9: 1\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\ndevice_key_old: hunter2\n")
    with pytest.raises(
        EsphomeError,
        match=r"^Could not check whether other configurations use this key: "
        r"Error reading file .*latin1\.yaml.*; only a key unique to this device",
    ):
        rotation_edits(OLD_KEY, NEW_KEY)


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


def test_two_bare_blocks_old_key_goes_into_the_last(tmp_path: Path) -> None:
    """Package entries merge in first, so the device's own block is the last."""
    head = """esphome:
  name: test

api:
  encryption:
    key: "{}"

ota:
  - platform: esphome
    encryption:
  - platform: esphome
    encryption:
"""
    path = _setup(tmp_path, head.format(OLD_KEY))
    _rotate()
    assert path.read_text() == head.format(NEW_KEY) + f'      old_key: "{OLD_KEY}"\n'


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


def test_secret_beside_a_symlinked_main_config(tmp_path: Path) -> None:
    """The loader looks beside the config path as given, not its target."""
    target = tmp_path / "shared" / "test.yaml"
    target.parent.mkdir()
    target.write_bytes(SECRET_YAML.encode())
    (target.parent / "secrets.yaml").write_bytes(b"device_key: other\n")
    (tmp_path / "secrets.yaml").write_bytes(f"device_key: {OLD_KEY}\n".encode())
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    CORE.config_path.symlink_to(target)
    CORE.raw_config = yaml_util.load_yaml(CORE.config_path)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [e.path for e in edits] == [(tmp_path / "secrets.yaml").resolve()]


def test_secret_in_a_symlinked_include_resolves_beside_the_link(tmp_path: Path) -> None:
    """The loader looks beside the include as written, not beside its target."""
    shared = tmp_path / "shared"
    shared.mkdir()
    (shared / "api.yaml").write_bytes(b"encryption:\n  key: !secret device_key\n")
    (shared / "secrets.yaml").write_bytes(b"device_key: other\n")
    sub = tmp_path / "sub"
    sub.mkdir()
    (sub / "api.yaml").symlink_to(shared / "api.yaml")
    (sub / "secrets.yaml").write_bytes(f"device_key: {OLD_KEY}\n".encode())
    _setup(
        tmp_path,
        _device("sub/api.yaml"),
    )
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [e.path for e in edits] == [(sub / "secrets.yaml").resolve()]


def test_scan_reports_what_it_could_not_read(tmp_path: Path) -> None:
    """An unreadable file or a linked directory makes the shared-use check
    incomplete, so the rotation is refused."""
    (tmp_path / "latin1.yaml").write_bytes(b"caf\xe9: 1\n")
    elsewhere = tmp_path.parent / "elsewhere_configs"
    elsewhere.mkdir(exist_ok=True)
    (tmp_path / "linked").symlink_to(elsewhere)
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\ndevice_key_old: x\n")
    _refused(
        "^Could not check whether other configurations use this key: "
        r".*linked is a link \(and 1 more\); only a key unique"
    )


def test_secret_used_elsewhere_in_this_configuration_is_refused(
    tmp_path: Path,
) -> None:
    """A wifi password on the key's secret would change with it."""
    _setup(
        tmp_path,
        SECRET_YAML + "wifi:\n  password: !secret device_key\n",
        f"device_key: {OLD_KEY}\n",
    )
    _refused("^secret 'device_key' is also used by test.yaml:13;")


def test_sharers_include_the_files_that_include_a_sharer(tmp_path: Path) -> None:
    """`other.yaml` reaches the secret through `base.yaml`."""
    (tmp_path / "base.yaml").write_bytes(
        b"api:\n  encryption:\n    key: !secret device_key\n"
    )
    (tmp_path / "other.yaml").write_bytes(b"packages:\n  base: !include base.yaml\n")
    (tmp_path / "third.yaml").write_bytes(b"packages:\n  base: !include other.yaml\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    _refused("^secret 'device_key' is also used by base.yaml, other.yaml, third.yaml;")


def test_old_secret_goes_to_the_secrets_file_of_the_block_that_gets_old_key(
    tmp_path: Path,
) -> None:
    """`!secret device_key_old` in the main file resolves from the main
    secrets; when the key only lives beside an include, that is refused
    rather than left to fail at compile time."""
    sub = tmp_path / "sub"
    sub.mkdir()
    (sub / "api.yaml").write_bytes(b"encryption:\n  key: !secret device_key\n")
    (sub / "secrets.yaml").write_bytes(f"device_key: {OLD_KEY}\n".encode())
    _setup(
        tmp_path,
        _device("sub/api.yaml"),
        "wifi: x\n",
    )
    with pytest.raises(
        EsphomeError, match="No plain 'device_key:' line in .*secrets.yaml"
    ):
        old_key_edit(OLD_KEY)


def test_a_commented_out_secret_use_does_not_count(tmp_path: Path) -> None:
    _setup(
        tmp_path,
        SECRET_YAML
        + "wifi:\n  # password: !secret device_key\n  password: x  # !secret device_key\n",
        f"device_key: {OLD_KEY}\n",
    )
    assert len(locate_key_edits(OLD_KEY, NEW_KEY)) == 1


def test_a_directory_include_is_reported_as_unchecked(tmp_path: Path) -> None:
    """`!include_dir_*` pulls in files by directory, which the name scan
    cannot follow."""
    (tmp_path / "other.yaml").write_bytes(b"packages: !include_dir_named packages\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    path = re.escape(str(tmp_path.resolve() / "other.yaml"))
    _refused(f": {path} includes the directory packages; only a key unique")


def test_a_hash_inside_a_secret_name_is_part_of_it(tmp_path: Path) -> None:
    """A comment needs whitespace before its `#`; `device_key#prod` is the name."""
    _setup(
        tmp_path,
        SECRET_YAML.replace("!secret device_key", "!secret device_key#prod"),
        f"device_key: other\ndevice_key#prod: {OLD_KEY}\n",
    )
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.line, e.secret) for e in edits] == [(1, "device_key#prod")]


def test_old_key_follows_the_ota_block_not_the_api_secret(tmp_path: Path) -> None:
    """An api key from a secret next to an ota block with its own literal
    key: the literal goes under the ota key line, no `_old` secret."""
    (tmp_path / "api.yaml").write_bytes(b"encryption:\n  key: !secret device_key\n")
    yaml_text = f"""esphome:
  name: test

api: !include api.yaml

ota:
  - platform: esphome
    encryption:
      key: "{OLD_KEY}"
"""
    path = _setup(tmp_path, yaml_text, f"device_key: {OLD_KEY}\n")
    _rotate()
    assert path.read_text() == yaml_text.replace(
        f'      key: "{OLD_KEY}"\n',
        f'      key: "{NEW_KEY}"\n      old_key: "{OLD_KEY}"\n',
    )
    assert (tmp_path / "secrets.yaml").read_text() == f"device_key: {NEW_KEY}\n"


def test_bare_block_after_an_api_key_without_a_line(tmp_path: Path) -> None:
    """An api key validation rebuilt has no line to read a secret from, so
    the inherited key is kept as a literal."""
    path = _setup(tmp_path, API_YAML)
    CORE.raw_config["api"]["encryption"] = {"key": OLD_KEY}
    (edit,) = old_key_edit(OLD_KEY)
    assert (edit.path, edit.new_line) == (path, f'      old_key: "{OLD_KEY}"')


def test_secret_name_with_spaces_is_not_a_plain_reference(tmp_path: Path) -> None:
    """`!secret ota key` is one name to the loader, not the secret `ota`."""
    _setup(
        tmp_path,
        SECRET_YAML.replace("!secret device_key", "!secret device key"),
        f"device key: {OLD_KEY}\n",
    )
    with pytest.raises(EsphomeError, match="edit the key by hand"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_comments_in_other_configurations_are_not_uses(tmp_path: Path) -> None:
    (tmp_path / "other.yaml").write_bytes(
        b"# key: !secret device_key\n# packages: !include_dir_named packages\nwifi: x\n"
    )
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    assert rotation_edits(OLD_KEY, NEW_KEY)


def test_existing_old_key_secret_used_elsewhere_is_refused(tmp_path: Path) -> None:
    yaml_text = (
        SECRET_YAML.replace(
            "      key: !secret device_key\n",
            "      key: !secret device_key\n      old_key: !secret device_key_old\n",
        )
        + "wifi:\n  password: !secret device_key_old\n"
    )
    _setup(tmp_path, yaml_text, f"device_key: {OLD_KEY}\ndevice_key_old: {OLDER_KEY}\n")
    _refused("^secret 'device_key_old' is also used by test.yaml:14;")


def test_a_key_merged_from_an_anchor_is_refused(tmp_path: Path) -> None:
    """The anchor's line may be merged into other mappings as well."""
    _setup(
        tmp_path,
        f'named: &named\n  key: "{OLD_KEY}"\n\napi:\n  encryption:\n    <<: *named\n\n'
        "ota:\n  - platform: esphome\n    encryption:\n",
    )
    with pytest.raises(EsphomeError, match="not on a line of its own"):
        locate_key_edits(OLD_KEY, NEW_KEY)


def test_old_key_inserted_into_a_shared_include_names_the_sharers(
    tmp_path: Path,
) -> None:
    """The ota block lives in an include another device loads too, so the
    inserted old_key reaches that device at its next install."""
    (tmp_path / "ota.yaml").write_bytes(
        f'platform: esphome\nencryption:\n  key: "{OLD_KEY}"\n'.encode()
    )
    (tmp_path / "other.yaml").write_bytes(b"ota:\n  - !include ota.yaml\n")
    _setup(tmp_path, "esphome:\n  name: test\n\nota:\n  - !include ota.yaml\n")
    with pytest.raises(
        EsphomeError, match="^ota.yaml is also used by other.yaml; only a key unique"
    ):
        rotation_edits(OLD_KEY, NEW_KEY)


def test_a_device_including_this_config_refuses_every_edit_it_shares(
    tmp_path: Path,
) -> None:
    """The secret and the file that gets old_key are both named."""
    (tmp_path / "other.yaml").write_bytes(b"packages:\n  base: !include test.yaml\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    with pytest.raises(
        EsphomeError,
        match="^secret 'device_key', test.yaml are also used by other.yaml;",
    ):
        rotation_edits(OLD_KEY, NEW_KEY)
    assert (tmp_path / "secrets.yaml").read_text() == f"device_key: {OLD_KEY}\n"


def test_old_key_going_into_a_package_other_devices_use_is_refused(
    tmp_path: Path,
) -> None:
    """The key is this device's own, but old_key would land in a shared package."""
    (tmp_path / "ota.yaml").write_bytes(b"platform: esphome\nencryption:\n")
    (tmp_path / "other.yaml").write_bytes(b"ota:\n  - !include ota.yaml\n")
    _setup(
        tmp_path,
        f'esphome:\n  name: test\n\napi:\n  encryption:\n    key: "{OLD_KEY}"\n\n'
        "ota:\n  - !include ota.yaml\n",
    )
    with pytest.raises(EsphomeError, match="^ota.yaml is also used by other.yaml;"):
        rotation_edits(OLD_KEY, NEW_KEY)


def test_key_overridden_on_top_of_a_package_block(tmp_path: Path) -> None:
    """The device yaml's line wins the merge and is the one rewritten."""
    from esphome.config_helpers import merge_config

    (tmp_path / "package.yaml").write_bytes(
        f'api:\n  encryption:\n    key: "{OLDER_KEY}"\n'.encode()
    )
    path = _setup(tmp_path, API_YAML)
    package = yaml_util.load_yaml(tmp_path / "package.yaml")
    CORE.raw_config = merge_config(package, CORE.raw_config)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path, e.line) for e in edits] == [(path, 5)]


def test_a_substituted_include_path_is_reported_as_unchecked(tmp_path: Path) -> None:
    (tmp_path / "other.yaml").write_bytes(b"packages:\n  base: !include ${common}\n")
    (tmp_path / "third.yaml").write_bytes(
        b"packages:\n  base: !include\n    file: ${pkg}\n"
    )
    # A remote package's file is never a local include
    (tmp_path / "remote.yaml").write_bytes(
        b"packages:\n  base:\n    url: x\n    file: ${pkg}\n"
    )
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    _refused(
        r"other\.yaml includes \$\{common\}, a path only known when loaded \(and 1 more\);"
    )


@pytest.mark.parametrize(
    ("include", "shown"),
    [
        ("!include <% path %>", "<% path %>"),
        ("!include\n    file: !env_var PKG", "!env_var PKG"),
        ("!include\n    file: !secret pkg_path", "!secret pkg_path"),
        ("!include *pkg", "*pkg"),
        ("!include\n    file: *pkg", "*pkg"),
    ],
)
def test_an_include_path_known_only_when_loaded_is_unchecked(
    tmp_path: Path, include: str, shown: str
) -> None:
    (tmp_path / "other.yaml").write_text(
        f"x: &pkg test.yaml\npackages:\n  base: {include}\n", newline="\n"
    )
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    _refused(
        rf"other\.yaml includes {re.escape(shown)}, a path only known when loaded;"
    )


@pytest.mark.parametrize(
    "value", ["!secret &saved device_key", "&saved !secret device_key"]
)
def test_an_anchored_secret_use_is_found(tmp_path: Path, value: str) -> None:
    (tmp_path / "other.yaml").write_text(f"wifi:\n  password: {value}\n", newline="\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    _refused(r"^secret 'device_key' is also used by other\.yaml;")


def test_an_anchored_include_is_followed(tmp_path: Path) -> None:
    (tmp_path / "other.yaml").write_bytes(b"packages:\n  base: !include &p test.yaml\n")
    _setup(tmp_path, API_YAML)
    _refused("^test.yaml is also used by other.yaml;")


def test_a_device_including_the_main_file_shares_its_included_key(
    tmp_path: Path,
) -> None:
    """The key comes through the main file's include_dir, which the scan does not
    link; the other device loads the main file, so it loads that key too."""
    (tmp_path / "ota").mkdir()
    (tmp_path / "ota" / "esphome.yaml").write_bytes(
        f'- platform: esphome\n  encryption:\n    key: "{OLD_KEY}"\n'.encode()
    )
    (tmp_path / "other.yaml").write_bytes(b"packages:\n  base: !include test.yaml\n")
    _setup(tmp_path, "esphome:\n  name: test\n\nota: !include_dir_merge_list ota\n")
    _refused(r"^test\.yaml is also used by other\.yaml;")


def test_another_device_including_the_main_file_is_a_sharer(tmp_path: Path) -> None:
    (tmp_path / "other.yaml").write_bytes(b"packages:\n  base: !include test.yaml\n")
    _setup(tmp_path, API_YAML)
    _refused("^test.yaml is also used by other.yaml;")


def test_mapping_includes_are_sharers_and_remote_files_are_not(
    tmp_path: Path,
) -> None:
    (tmp_path / "block.yaml").write_bytes(
        b"packages:\n  base: !include\n    file: test.yaml\n    vars: {a: 1}\n"
    )
    (tmp_path / "flow.yaml").write_bytes(b"packages:\n  - !include {file: test.yaml}\n")
    (tmp_path / "remote.yaml").write_bytes(
        b"packages:\n  base:\n    url: https://example.com/x\n    file: test.yaml\n"
    )
    (tmp_path / "plain.yaml").write_bytes(b"esphome:\n  name: test.yaml\n")
    _setup(tmp_path, API_YAML)
    _refused("^test.yaml is also used by block.yaml, flow.yaml;")


def test_hidden_trees_are_not_scanned(tmp_path: Path) -> None:
    (tmp_path / ".venv").mkdir()
    (tmp_path / ".venv" / "other.yaml").write_bytes(
        b"packages:\n  base: !include ../test.yaml\n"
    )
    (tmp_path / ".hidden.yaml").write_bytes(b": [unreadable\n")
    _setup(tmp_path, API_YAML)
    assert rotation_edits(OLD_KEY, NEW_KEY)


def test_secret_key_overridden_on_top_of_a_package_block(tmp_path: Path) -> None:
    """A winning `key: !secret` line of a merged block is edited like a literal one."""
    from esphome.config_helpers import merge_config

    (tmp_path / "package.yaml").write_bytes(
        f'api:\n  encryption:\n    key: "{OLDER_KEY}"\n'.encode()
    )
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    package = yaml_util.load_yaml(tmp_path / "package.yaml")
    CORE.raw_config = merge_config(package, CORE.raw_config)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [(e.path.name, e.secret) for e in edits] == [("secrets.yaml", "device_key")]


def test_secret_uses_before_the_last_line_and_in_flow_mappings(tmp_path: Path) -> None:
    """A use is found on any line of another file and inside a flow mapping."""
    (tmp_path / "early.yaml").write_bytes(
        b"api:\n  encryption:\n    key: !secret device_key\nlogger:\n"
    )
    (tmp_path / "flow.yaml").write_bytes(
        b"api: {encryption: {key: !secret device_key}}\n"
    )
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    _refused("^secret 'device_key' is also used by early.yaml, flow.yaml;")


def test_secret_used_in_a_flow_mapping_of_this_configuration_is_refused(
    tmp_path: Path,
) -> None:
    _setup(
        tmp_path,
        SECRET_YAML + "wifi: {password: !secret device_key}\n",
        f"device_key: {OLD_KEY}\n",
    )
    _refused("^secret 'device_key' is also used by test.yaml:12;")


def test_a_device_including_the_main_file_shares_its_secret(tmp_path: Path) -> None:
    """The main file names the secret, so a device including it is a sharer."""
    (tmp_path / "other.yaml").write_bytes(b"packages:\n  base: !include test.yaml\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    _refused("^secret 'device_key', test.yaml are also used by other.yaml;")


def test_repeated_old_key_entries_are_all_rewritten(tmp_path: Path) -> None:
    """Same-port entries must keep one old_key, so each source is rewritten."""
    yaml_text = f"""esphome:
  name: test

ota:
  - platform: esphome
    encryption:
      old_key: "{OLDER_KEY}"
  - platform: esphome
    encryption:
      key: "{OLD_KEY}"
      old_key: "{OLDER_KEY}"
"""
    path = _setup(tmp_path, yaml_text)
    _rotate()
    assert path.read_text() == yaml_text.replace(
        f'key: "{OLD_KEY}"', f'key: "{NEW_KEY}"'
    ).replace(f'old_key: "{OLDER_KEY}"', f'old_key: "{OLD_KEY}"')


def test_repeated_old_key_secret_is_rewritten_once(tmp_path: Path) -> None:
    yaml_text = f"""esphome:
  name: test

ota:
  - platform: esphome
    encryption:
      old_key: !secret device_old
  - platform: esphome
    encryption:
      key: "{OLD_KEY}"
      old_key: !secret device_old
"""
    _setup(tmp_path, yaml_text, f"device_old: {OLDER_KEY}\n")
    edits = old_key_edit(OLD_KEY)
    assert [(e.path.name, e.new_line) for e in edits] == [
        ("secrets.yaml", f"device_old: {OLD_KEY}")
    ]


def test_merged_secret_key_with_a_hash_in_its_name(tmp_path: Path) -> None:
    """A merged block's `key: !secret a#b` line locates like an unmerged one."""
    from esphome.config_helpers import merge_config

    (tmp_path / "package.yaml").write_bytes(
        f'api:\n  encryption:\n    key: "{OLDER_KEY}"\n'.encode()
    )
    _setup(
        tmp_path,
        SECRET_YAML.replace("device_key", "device_key#prod"),
        f"device_key#prod: {OLD_KEY}\n",
    )
    package = yaml_util.load_yaml(tmp_path / "package.yaml")
    CORE.raw_config = merge_config(package, CORE.raw_config)
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [e.secret for e in edits] == ["device_key#prod"]


def test_bare_block_located_after_full_validation(tmp_path: Path) -> None:
    """Validation rewrites the raw ota list; the pre-validation snapshot keeps its lines."""
    from esphome.config import read_config

    yaml_text = API_YAML.replace(
        "esphome:\n  name: test\n",
        "esphome:\n  name: test\nesp8266:\n  board: d1_mini\n"
        "wifi:\n  ssid: x\n  password: yyyyyyyy\n",
    )
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    CORE.config_path.write_bytes(yaml_text.encode())
    config = read_config({}, snapshot_user_config=True)
    assert config is not None
    CORE.raw_config = config.user_config
    edits = rotation_edits(OLD_KEY, NEW_KEY)
    assert [e.new_line.strip() for e in edits] == [
        f'key: "{NEW_KEY}"  # shared with ota',
        f'old_key: "{OLD_KEY}"',
    ]


def test_secret_key_with_a_literal_old_key_is_refused(tmp_path: Path) -> None:
    """The key from secrets.yaml must not be copied into the yaml as old_key."""
    _setup(
        tmp_path,
        SECRET_YAML + f'      old_key: "{OLDER_KEY}"\n',
        f"device_key: {OLD_KEY}\n",
    )
    with pytest.raises(EsphomeError, match="kept in secrets.yaml"):
        old_key_edit(OLD_KEY)


def test_secret_key_from_another_entry_with_a_literal_insert_is_refused(
    tmp_path: Path,
) -> None:
    """A keyless entry next to one keyed from a secret gets no plain old_key."""
    yaml_text = f"""esphome:
  name: test

api:
  encryption:
    key: "{OLDER_KEY}"

ota:
  - platform: esphome
    encryption:
  - platform: esphome
    encryption:
      key: !secret device_key
"""
    _setup(tmp_path, yaml_text, f"device_key: {OLD_KEY}\n")
    edits = locate_key_edits(OLD_KEY, NEW_KEY)
    assert [e.secret for e in edits] == ["device_key"]
    # The keyed entry takes old_key, as a secret
    assert [e.new_line.strip() for e in old_key_edit(OLD_KEY)] == [
        "old_key: !secret device_key_old",
        f'device_key_old: "{OLD_KEY}"',
    ]


@pytest.mark.parametrize(
    ("block", "expected"),
    [
        ('    "encryption":\n', '    "encryption":\n      old_key: "{key}"\n'),
        (
            "    encryption:\n        allow_plaintext_upload: false\n",
            '    encryption:\n        old_key: "{key}"\n        allow_plaintext_upload: false\n',
        ),
    ],
    ids=["quoted", "existing_option"],
)
def test_bare_block_old_key_shape(tmp_path: Path, block: str, expected: str) -> None:
    # Only the ota block's `encryption:` line changes; the api one stays
    yaml_text = API_YAML.rsplit("    encryption:\n", 1)[0] + block
    path = _setup(tmp_path, yaml_text)
    _rotate()
    assert path.read_text() == yaml_text.replace(OLD_KEY, NEW_KEY).replace(
        block, expected.format(key=OLD_KEY)
    )


def test_quoted_key_field_gets_old_key(tmp_path: Path) -> None:
    yaml_text = f"""esphome:
  name: test

ota:
  - platform: esphome
    encryption:
      "key": "{OLD_KEY}"
"""
    path = _setup(tmp_path, yaml_text)
    _rotate()
    assert path.read_text() == yaml_text.replace(
        f'"key": "{OLD_KEY}"\n', f'"key": "{NEW_KEY}"\n      old_key: "{OLD_KEY}"\n'
    )


def test_secret_use_after_an_escaped_quote_is_refused(tmp_path: Path) -> None:
    _setup(
        tmp_path,
        SECRET_YAML + 'wifi: {ssid: "Kitchen \\" #1", password: !secret device_key}\n',
        f"device_key: {OLD_KEY}\n",
    )
    _refused("^secret 'device_key' is also used by test.yaml:12;")


def test_secret_named_on_the_next_line_is_refused(tmp_path: Path) -> None:
    _setup(
        tmp_path,
        SECRET_YAML + "wifi:\n  password: !secret\n    device_key\n",
        f"device_key: {OLD_KEY}\n",
    )
    _refused("^secret 'device_key' is also used by test.yaml:13;")


@pytest.mark.skipif(sys.platform == "win32", reason="symlinks need privileges")
def test_device_including_the_target_of_a_linked_include_is_a_sharer(
    tmp_path: Path,
) -> None:
    """The key is read through `alias.yaml`; `other.yaml` includes its target."""
    (tmp_path / "common.yaml").write_bytes(
        f'encryption:\n  key: "{OLD_KEY}"\n'.encode()
    )
    (tmp_path / "alias.yaml").symlink_to(tmp_path / "common.yaml")
    (tmp_path / "other.yaml").write_bytes(b"api: !include common.yaml\n")
    _setup(tmp_path, _device("alias.yaml"))
    _refused(" is also used by other.yaml;")


def test_a_file_that_fails_to_scan_is_unchecked(tmp_path: Path) -> None:
    """Its uses cannot be read, so the rotation is refused naming it."""
    (tmp_path / "broken.yaml").write_bytes(b"key: !secret device_key\nbad: @x\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\ndevice_key_old: x\n")
    _refused(r": Could not scan .*broken\.yaml.*; only a key unique")


def test_includes_are_matched_by_resolved_path_outside_comments(tmp_path: Path) -> None:
    """Only a path that resolves to the edited file counts: not `wifi_api.yaml`, a
    commented include, a remote package or another folder's `api.yaml`."""
    (tmp_path / "api.yaml").write_bytes(f'encryption:\n  key: "{OLD_KEY}"\n'.encode())
    (tmp_path / "lookalike.yaml").write_bytes(
        b"wifi: !include wifi_api.yaml\n# api: !include api.yaml\n"
    )
    (tmp_path / "remote.yaml").write_bytes(
        b"packages:\n  base: github://me/configs/api.yaml@main\n"
    )
    (tmp_path / "other").mkdir()
    (tmp_path / "other" / "api.yaml").write_bytes(b"encryption:\n")
    (tmp_path / "other" / "device.yaml").write_bytes(b"api: !include api.yaml\n")
    (tmp_path / "other" / "parent.yaml").write_bytes(b"api: !include ../api.yaml\n")
    _setup(
        tmp_path,
        _device("api.yaml"),
    )
    _refused("^api.yaml is also used by other/parent.yaml;")


def test_bare_block_followed_by_a_sibling_gets_one_level_more(tmp_path: Path) -> None:
    """Blank and comment lines are passed over; a sibling option is not a child."""
    yaml_text = API_YAML + "\n    # the port\n    port: 3232\n"
    path = _setup(tmp_path, yaml_text)
    (edit,) = old_key_edit(OLD_KEY)
    assert (edit.path, edit.line, edit.new_line) == (
        path,
        9,
        f'      old_key: "{OLD_KEY}"',
    )


def test_literal_old_key_next_to_a_secret_key_entry_is_refused(tmp_path: Path) -> None:
    """The entry with its own key line gets old_key; a plain copy of a secret is refused."""
    yaml_text = f"""esphome:
  name: test

ota:
  - platform: esphome
    encryption:
      key: "{OLDER_KEY}"
  - platform: esphome
    encryption:
      key: !secret device_key
"""
    _setup(tmp_path, yaml_text, f"device_key: {OLD_KEY}\n")
    with pytest.raises(EsphomeError, match="kept in secrets.yaml"):
        old_key_edit(OLD_KEY)


def test_old_secret_used_by_a_package_in_the_build_data_is_refused(
    tmp_path: Path,
) -> None:
    """A file this config loads from outside the scanned tree still counts."""
    (tmp_path / ".esphome").mkdir()
    (tmp_path / ".esphome" / "pkg.yaml").write_bytes(
        b"ssid: x\npassword: !secret device_key_old\n"
    )
    _setup(
        tmp_path,
        SECRET_YAML + "wifi: !include .esphome/pkg.yaml\n",
        f"device_key: {OLD_KEY}\ndevice_key_old: {OLDER_KEY}\n",
    )
    _refused(r"^secret 'device_key_old' is also used by \.esphome/pkg\.yaml:2;")


def test_secret_in_an_include_outside_the_config_dir_is_refused(tmp_path: Path) -> None:
    """Another device's include outside the scanned tree is followed, includes and all."""
    (shared := tmp_path / "shared").mkdir()
    (shared / "api.yaml").write_bytes(b"<<: !include deeper.yaml\n")
    (shared / "deeper.yaml").write_bytes(b"key: !secret device_key\n")
    (config_dir := tmp_path / "configs").mkdir()
    (config_dir / "other.yaml").write_bytes(
        b"api: !include ../shared/api.yaml\nwifi: !include ../shared/api.yaml\n"
    )
    _setup(config_dir, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    deeper = re.escape((shared / "deeper.yaml").resolve().as_posix())
    _refused(rf"^secret 'device_key' is also used by .*{deeper}, other\.yaml;")


@pytest.mark.skipif(
    sys.platform == "win32", reason="symlinks need privileges on Windows"
)
def test_a_linked_include_follows_includes_beside_the_link(tmp_path: Path) -> None:
    """The loader resolves a linked file's includes from the link's directory."""
    (real := tmp_path / "real").mkdir()
    (real / "pkg.yaml").write_bytes(b"<<: !include nested.yaml\n")
    (real / "nested.yaml").write_bytes(b"x: 1\n")
    (shared := tmp_path / "shared").mkdir()
    (shared / "pkg.yaml").symlink_to(real / "pkg.yaml")
    (shared / "nested.yaml").write_bytes(b"key: !secret device_key\n")
    (config_dir := tmp_path / "configs").mkdir()
    (config_dir / "other.yaml").write_bytes(b"api: !include ../shared/pkg.yaml\n")
    _setup(config_dir, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    nested = re.escape((shared / "nested.yaml").resolve().as_posix())
    _refused(rf"^secret 'device_key' is also used by .*{nested}")


def test_an_include_cycle_ends(tmp_path: Path) -> None:
    (loop := tmp_path / "loop").mkdir()
    (loop / "a.yaml").write_bytes(b"x: !include ../loop/a.yaml\n")
    (tmp_path / "other.yaml").write_bytes(b"x: !include loop/a.yaml\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    assert rotation_edits(OLD_KEY, NEW_KEY)


def test_a_missing_include_target_is_unchecked(tmp_path: Path) -> None:
    (tmp_path / "other.yaml").write_bytes(b"api: !include ../gone.yaml\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    _refused(
        r"^Could not check whether other configurations use this key: Error reading"
    )


def test_secret_used_by_a_remote_package_checkout_is_refused(tmp_path: Path) -> None:
    """Another device's remote package falls back to this secrets.yaml."""
    checkout = tmp_path / ".esphome" / "packages" / "abc123"
    checkout.mkdir(parents=True)
    (checkout / "base.yaml").write_bytes(b"ota:\n  key: !secret device_key\n")
    (checkout / ".git").mkdir()
    (checkout / ".git" / "broken.yaml").write_bytes(b": [unreadable\n")
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    _refused(
        r"^secret 'device_key' is also used by \.esphome/packages/abc123/base\.yaml;"
    )


def test_secret_used_by_an_own_file_outside_the_config_dir_is_refused(
    tmp_path: Path,
) -> None:
    """A use outside the config dir is named by its full path."""
    (tmp_path / "pkg.yaml").write_bytes(b"ssid: x\npassword: !secret device_key\n")
    (config_dir := tmp_path / "configs").mkdir()
    _setup(
        config_dir,
        SECRET_YAML + "wifi: !include ../pkg.yaml\n",
        f"device_key: {OLD_KEY}\n",
    )
    pkg = re.escape((tmp_path / "pkg.yaml").resolve().as_posix())
    _refused(f"^secret 'device_key' is also used by {pkg}:2;")


@pytest.mark.skipif(sys.platform == "win32", reason="symlinks need privileges")
def test_linked_build_data_dir_is_not_unchecked(tmp_path: Path) -> None:
    (tmp_path / "data").mkdir()
    (tmp_path / ".esphome").symlink_to(tmp_path / "data")
    _setup(tmp_path, API_YAML)
    assert rotation_edits(OLD_KEY, NEW_KEY)


def test_secret_users_in_any_directory_count(tmp_path: Path) -> None:
    """A same-named secret counts whichever secrets.yaml a device reads it from."""
    secrets_files = {"own": b"device_key: other\n", "broken": b"device_key: [\n"}
    for folder in ("own", "fallback", "broken"):
        (tmp_path / folder).mkdir()
        (tmp_path / folder / "device.yaml").write_bytes(
            b"api:\n  encryption:\n    key: !secret device_key\n"
        )
        if (secrets := secrets_files.get(folder)) is not None:
            (tmp_path / folder / "secrets.yaml").write_bytes(secrets)
    _setup(tmp_path, SECRET_YAML, f"device_key: {OLD_KEY}\n")
    _refused(
        r"^secret 'device_key' is also used by broken/device\.yaml, "
        r"fallback/device\.yaml, own/device\.yaml; only a key unique to this "
        "device can be rotated, change it by hand$"
    )


@pytest.mark.skipif(sys.platform == "win32", reason="symlinks need privileges")
def test_one_line_read_through_a_link_and_its_target_is_edited_once(
    tmp_path: Path,
) -> None:
    (tmp_path / "enc.yaml").write_bytes(f'key: "{OLD_KEY}"\n'.encode())
    (tmp_path / "link.yaml").symlink_to(tmp_path / "enc.yaml")
    yaml_text = """esphome:
  name: test

ota:
  - platform: esphome
    encryption: !include enc.yaml
  - platform: esphome
    encryption: !include link.yaml
"""
    _setup(tmp_path, yaml_text)
    assert len(locate_key_edits(OLD_KEY, NEW_KEY)) == 1


def test_encryption_shared_with_the_api_gets_no_old_key(tmp_path: Path) -> None:
    """The api encryption schema rejects old_key, so a shared mapping is refused."""
    (tmp_path / "enc.yaml").write_bytes(f'key: "{OLD_KEY}"\n'.encode())
    yaml_text = """esphome:
  name: test

api:
  encryption: !include enc.yaml

ota:
  - platform: esphome
    encryption: !include enc.yaml
"""
    _setup(tmp_path, yaml_text)
    with pytest.raises(EsphomeError, match="is the api key too"):
        old_key_edit(OLD_KEY)
