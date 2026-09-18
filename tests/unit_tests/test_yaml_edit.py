"""Tests for rewriting single lines of a yaml file in place."""

from __future__ import annotations

from pathlib import Path
import sys
from unittest.mock import patch

import pytest

from esphome import yaml_util
from esphome.compiled_config import compiled_config_path
from esphome.config import do_substitution_pass
from esphome.const import CONF_ESPHOME, CONF_NAME
from esphome.core import CORE, EsphomeError
from esphome.yaml_edit import (
    LineEdit,
    Snapshot,
    apply_line_edits,
    field_line_re,
    line_at,
    restore_files,
    rewrite,
    rewritten_text,
    secret_insert,
    secret_line,
    secret_rewrite,
    secrets_path_for,
    source_line,
)

YAML = """esphome:
  name: kitchen  # the device

wifi:
  ssid: kitchen
"""


def _setup(tmp_path: Path, yaml_text: str, secrets: str | None = None) -> Path:
    """Write the yaml (and secrets.yaml), point CORE at it and load it the way
    read_config does, so every node carries its source range."""
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    # Bytes, so Windows does not turn the newlines into CRLF on the way in
    CORE.config_path.write_bytes(yaml_text.encode())
    if secrets is not None:
        (tmp_path / "secrets.yaml").write_bytes(secrets.encode())
    CORE.raw_config = do_substitution_pass(yaml_util.load_yaml(CORE.config_path), None)
    return CORE.config_path


def _name_edit(new_name: str) -> LineEdit:
    doc, line_no, text = source_line(CORE.raw_config[CONF_ESPHOME], CONF_NAME)
    match = field_line_re(CONF_NAME, "kitchen").match(text)
    return LineEdit(doc, line_no, text, rewrite(match, new_name))


def test_rewrite_keeps_the_rest_of_the_line(tmp_path: Path) -> None:
    """The value changes; indentation, quotes and the comment stay."""
    path = _setup(tmp_path, YAML.replace("name: kitchen", "name: 'kitchen'"))
    edit = _name_edit("garage")
    assert (edit.line, edit.new_line) == (1, "  name: 'garage'  # the device")
    snapshots = apply_line_edits([edit])
    assert path.read_text() == YAML.replace("name: kitchen", "name: 'garage'")
    assert snapshots == {
        path: Snapshot(
            YAML.replace("name: kitchen", "name: 'kitchen'"), path.read_text()
        )
    }


def test_rewrite_can_force_quotes() -> None:
    match = field_line_re(CONF_NAME, "kitchen").match("  name: kitchen  # x")
    assert rewrite(match, "garage", quote='"') == '  name: "garage"  # x'


def test_only_the_located_line_changes(tmp_path: Path) -> None:
    """A lookalike `name:` under another block has its own range."""
    path = _setup(
        tmp_path, YAML + "sensor:\n  - platform: template\n    name: kitchen\n"
    )
    apply_line_edits([_name_edit("garage")])
    assert path.read_text().endswith("    name: kitchen\n")
    assert "  name: garage  # the device" in path.read_text()


def test_line_endings_are_kept(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML.replace("\n", "\r\n"))
    edit = _name_edit("garage")
    insert = LineEdit(
        path, edit.line, edit.old_line, "  comment: added", insert_after=True
    )
    apply_line_edits([edit, insert])
    assert path.read_bytes() == (
        b"esphome:\r\n  name: garage  # the device\r\n  comment: added\r\n\r\nwifi:\r\n  ssid: kitchen\r\n"
    )


def test_insert_after_the_last_line_without_a_newline() -> None:
    text = "esphome:\n  name: kitchen"
    edit = LineEdit(
        Path("x"), 1, "  name: kitchen", "  comment: added", insert_after=True
    )
    assert rewritten_text(text, [edit]) == "esphome:\n  name: kitchen\n  comment: added"


def test_stale_line_is_refused(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    edit = _name_edit("garage")
    path.write_bytes(YAML.replace("kitchen  #", "pantry  #").encode())
    with pytest.raises(EsphomeError, match="changed since it was read"):
        apply_line_edits([edit])
    path.write_bytes(b"esphome:\n")
    with pytest.raises(EsphomeError, match="changed since it was read"):
        apply_line_edits([edit])
    with pytest.raises(EsphomeError, match="changed since it was read"):
        line_at(path, 5)


def test_a_file_that_no_longer_loads_is_rolled_back(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    edit = _name_edit("garage")
    edit.new_line = "  name: [broken"
    with pytest.raises(EsphomeError, match="no longer loads"):
        apply_line_edits([edit])
    assert path.read_text() == YAML


def test_rollback_reports_a_restore_that_also_failed(tmp_path: Path) -> None:
    """The rewrite lands, the reload fails, and the rollback write fails too."""
    from esphome.yaml_edit import write_file

    _setup(tmp_path, YAML)
    edit = _name_edit("garage")
    writes: list[int] = []

    def write_then_fail(*args: object, **kwargs: object) -> None:
        writes.append(1)
        if len(writes) > 1:
            raise EsphomeError("disk")
        write_file(*args, **kwargs)

    with (
        patch("esphome.yaml_util.load_yaml", side_effect=EsphomeError("broken")),
        patch("esphome.yaml_edit.write_file", side_effect=write_then_fail),
        pytest.raises(EsphomeError, match="broken; Could not restore .*disk"),
    ):
        apply_line_edits([edit])


def test_rolls_back_on_an_interrupt(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    with (
        patch("esphome.yaml_util.load_yaml", side_effect=KeyboardInterrupt),
        pytest.raises(KeyboardInterrupt),
    ):
        apply_line_edits([_name_edit("garage")])
    assert path.read_text() == YAML


def test_restore_leaves_a_file_the_user_changed_alone(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    snapshots = apply_line_edits([_name_edit("garage")])
    edited = path.read_text() + "logger:\n"
    path.write_bytes(edited.encode())
    with pytest.raises(EsphomeError, match="changed since it was written, left as is"):
        restore_files(snapshots)
    assert path.read_text() == edited


def test_restore_reports_every_file_it_could_not_write(tmp_path: Path) -> None:
    _setup(tmp_path, YAML)
    with pytest.raises(
        EsphomeError, match="Could not restore .*gone.yaml: Error reading"
    ):
        restore_files({tmp_path / "gone.yaml": Snapshot("x")})


def test_restore_reports_a_cache_it_could_not_drop(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    with (
        patch(
            "esphome.compiled_config.invalidate_compiled_config",
            side_effect=EsphomeError("busy"),
        ),
        pytest.raises(EsphomeError, match="Could not restore busy"),
    ):
        restore_files({path: Snapshot(YAML)})


def test_clears_the_validated_cache(tmp_path: Path) -> None:
    _setup(tmp_path, YAML)
    cache = compiled_config_path(CORE.config_filename)
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text("{}")
    snapshots = apply_line_edits([_name_edit("garage")])
    assert not cache.exists()
    cache.write_text("{}")
    restore_files(snapshots)
    assert not cache.exists()


@pytest.mark.skipif(sys.platform == "win32", reason="posix file modes")
def test_keeps_the_file_mode(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    path.chmod(0o600)
    snapshots = apply_line_edits([_name_edit("garage")])
    assert path.stat().st_mode & 0o777 == 0o600
    restore_files(snapshots)
    assert path.stat().st_mode & 0o777 == 0o600


def test_mode_failure_is_reported(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    with (
        patch("pathlib.Path.chmod", side_effect=OSError("denied")),
        pytest.raises(EsphomeError, match="Could not keep the mode"),
    ):
        apply_line_edits([_name_edit("garage")])
    assert path.read_text() == YAML


def test_source_line_refuses_an_uneditable_source(tmp_path: Path) -> None:
    """A value validation added has no range; a file under the build data
    or outside the configuration directory is not the user's."""
    _setup(tmp_path, YAML)
    with pytest.raises(EsphomeError, match="was not read from a file"):
        source_line({"name": "kitchen"}, "name")
    outside = tmp_path.parent / "elsewhere.yaml"
    outside.write_bytes(b"esphome:\n  name: kitchen\n")
    try:
        (tmp_path / "test.yaml").unlink()
        (tmp_path / "test.yaml").symlink_to(outside)
        CORE.raw_config = yaml_util.load_yaml(CORE.config_path)
        with pytest.raises(EsphomeError, match="not an editable file"):
            source_line(CORE.raw_config[CONF_ESPHOME], CONF_NAME)
    finally:
        outside.unlink()


def test_symlinked_file_is_edited_at_its_target(tmp_path: Path) -> None:
    target = tmp_path / "shared" / "test.yaml"
    target.parent.mkdir()
    target.write_bytes(YAML.encode())
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    CORE.config_path.symlink_to(target)
    CORE.raw_config = yaml_util.load_yaml(CORE.config_path)
    edit = _name_edit("garage")
    assert edit.path == target.resolve()
    apply_line_edits([edit])
    assert CORE.config_path.is_symlink()
    assert "name: garage" in target.read_text()


def test_secret_lines(tmp_path: Path) -> None:
    """An indented root, `key :` spacing, quoted names and comments are all
    matched; a block scalar is not a plain line."""
    secrets = "  # keys\n  'wifi' : hunter2  # keep\n  device_key: >-\n    abc\n"
    _setup(tmp_path, YAML, secrets)
    path = secrets_path_for(CORE.config_path)
    assert path == tmp_path / "secrets.yaml"
    edit = secret_rewrite(path, "wifi", "swordfish", expect="hunter2")
    assert (edit.line, edit.new_line) == (1, "  'wifi' : swordfish  # keep")
    assert secret_rewrite(path, "wifi", "x", expect="other") is None
    assert secret_rewrite(path, "device_key", "x") is None
    assert secret_line(path, "missing") is None
    insert = secret_insert(path, "wifi", "wifi_old", "hunter2")
    assert (insert.line, insert.new_line, insert.insert_after) == (
        1,
        '  wifi_old: "hunter2"',
        True,
    )
    with pytest.raises(EsphomeError, match="No plain 'device_key:' line"):
        secret_insert(path, "device_key", "x", "y")
    apply_line_edits([edit, insert])
    assert path.read_text() == (
        "  # keys\n  'wifi' : swordfish  # keep\n  wifi_old: \"hunter2\"\n  device_key: >-\n    abc\n"
    )


def test_secrets_path_beside_an_include_falls_back_to_the_main_one(
    tmp_path: Path,
) -> None:
    _setup(tmp_path, YAML, "wifi: x\n")
    include = tmp_path / "sub" / "part.yaml"
    include.parent.mkdir()
    assert secrets_path_for(include) == tmp_path / "secrets.yaml"
    (include.parent / "secrets.yaml").write_bytes(b"wifi: y\n")
    assert secrets_path_for(include) == include.parent / "secrets.yaml"
