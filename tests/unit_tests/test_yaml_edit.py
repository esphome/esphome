"""Tests for rewriting single lines of a yaml file."""

from __future__ import annotations

from pathlib import Path
import sys
from typing import Any
from unittest.mock import patch

import pytest

from esphome import yaml_util
from esphome.compiled_config import compiled_config_path
from esphome.config import do_substitution_pass
from esphome.const import CONF_ESPHOME, CONF_NAME
from esphome.core import CORE, EsphomeError
from esphome.yaml_edit import (
    LineEdit,
    RestoreError,
    Snapshot,
    apply_line_edits,
    editable_file,
    field_line_re,
    line_at,
    own_documents,
    read_text,
    restore_files,
    rewrite,
    rewritten_text,
    source_line,
    source_of,
    write_keeping_mode,
)

YAML = """esphome:
  name: kitchen  # the device

wifi:
  ssid: kitchen
"""


def _setup(tmp_path: Path, yaml_text: str) -> Path:
    """Write the yaml, point CORE at it and load it the way read_config does,
    so every node carries its source range."""
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    # Bytes, so Windows does not turn the newlines into CRLF on the way in
    CORE.config_path.write_bytes(yaml_text.encode())
    CORE.raw_config = do_substitution_pass(yaml_util.load_yaml(CORE.config_path), None)
    return CORE.config_path


def _name_edit(new_name: str) -> LineEdit:
    doc, line_no = source_of(CORE.raw_config[CONF_ESPHOME], CONF_NAME)
    text = line_at(doc, line_no)
    match = field_line_re(CONF_NAME, "kitchen").match(text)
    return LineEdit(doc, line_no, text, rewrite(match, new_name))


def test_rewrite_keeps_the_rest_of_the_line(tmp_path: Path) -> None:
    """The value changes; indentation, quotes and the comment stay."""
    path = _setup(tmp_path, YAML.replace("name: kitchen", "name: 'kitchen'"))
    edit = _name_edit("garage")
    assert (edit.line, edit.new_line) == (1, "  name: 'garage'  # the device")
    assert rewritten_text(read_text(path), [edit]) == YAML.replace(
        "name: kitchen", "name: 'garage'"
    )


def test_rewrite_can_force_quotes() -> None:
    match = field_line_re(CONF_NAME, "kitchen").match("  name: kitchen  # x")
    assert rewrite(match, "garage", quote='"') == '  name: "garage"  # x'


def test_only_the_located_line_changes(tmp_path: Path) -> None:
    """A lookalike `name:` under another block has its own range."""
    yaml_text = YAML + "sensor:\n  - platform: template\n    name: kitchen\n"
    path = _setup(tmp_path, yaml_text)
    text = rewritten_text(read_text(path), [_name_edit("garage")])
    assert text.endswith("    name: kitchen\n")
    assert "  name: garage  # the device" in text


def test_line_endings_are_kept(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML.replace("\n", "\r\n"))
    assert rewritten_text(read_text(path), [_name_edit("garage")]) == YAML.replace(
        "\n", "\r\n"
    ).replace("name: kitchen", "name: garage")


def test_stale_line_is_refused(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    edit = _name_edit("garage")
    with pytest.raises(EsphomeError, match="changed since it was read"):
        rewritten_text(YAML.replace("kitchen  #", "pantry  #"), [edit])
    with pytest.raises(EsphomeError, match="changed since it was read"):
        rewritten_text("esphome:\n", [edit])
    with pytest.raises(EsphomeError, match="changed since it was read"):
        line_at(path, 5)


def test_a_comment_needs_whitespace_and_a_scalar_is_not_empty() -> None:
    """`abc#def` is one value to the loader, and a bare `key:` heads a block."""
    assert field_line_re("key", "abc").match("key: abc#def") is None
    assert field_line_re("key").match("key:") is None
    assert field_line_re("key").match("key: abc  # c")["trail"] == "  # c"
    assert field_line_re("key", "abc#def").match("key: abc#def") is not None


def test_source_of_is_none_for_a_value_validation_added() -> None:
    """Only a key read from a file carries a range."""
    assert source_of({"name": "kitchen"}, "name") is None


def test_a_mapping_built_in_code_places_only_this_configurations_keys(
    tmp_path: Path,
) -> None:
    """merge_config rebuilds a mapping without a range; a value read for this
    configuration is still placed, one read from elsewhere is not."""
    _setup(tmp_path, YAML)
    own_key, own_value = next(iter(CORE.raw_config[CONF_ESPHOME].items()))
    assert source_of({own_key: own_value}, CONF_NAME) == (CORE.config_path, 1)
    # Only the value names the line that won; without a range it is refused
    assert source_of({own_key: "x"}, CONF_NAME) is None
    (tmp_path / "elsewhere.yaml").write_bytes(YAML.encode())
    other = yaml_util.load_yaml(tmp_path / "elsewhere.yaml")
    other_key, other_value = next(iter(other[CONF_ESPHOME].items()))
    assert source_of({other_key: other_value}, CONF_NAME) is None


def test_own_documents_include_a_file_that_only_gives_substitutions(
    tmp_path: Path,
) -> None:
    (tmp_path / "subs.yaml").write_bytes(b"substitutions:\n  x: y\n")
    _setup(tmp_path, YAML + "packages:\n  subs: !include subs.yaml\n")
    CORE.raw_config = {}  # what is left after substitutions are taken out
    assert (tmp_path / "subs.yaml").resolve() in own_documents()


def test_source_of_is_none_for_a_merged_key(tmp_path: Path) -> None:
    """A key a merge brought in points at the anchor, which other mappings
    may merge as well; it is not this mapping's own line."""
    _setup(
        tmp_path,
        "named: &named\n  name: kitchen\n\nesphome:\n  <<: *named\n  friendly_name: x\n",
    )
    assert source_of(CORE.raw_config[CONF_ESPHOME], CONF_NAME) is None
    assert source_of(CORE.raw_config[CONF_ESPHOME], "friendly_name") == (
        tmp_path / "test.yaml",
        5,
    )


def test_source_of_names_the_file_the_loader_read(tmp_path: Path) -> None:
    """An include has its own document; a symlink is reported as given."""
    (tmp_path / "base.yaml").write_bytes(b"name: kitchen\n")
    _setup(tmp_path, "esphome: !include base.yaml\n")
    assert source_of(CORE.raw_config[CONF_ESPHOME], CONF_NAME) == (
        tmp_path / "base.yaml",
        0,
    )
    target = tmp_path / "shared" / "test.yaml"
    target.parent.mkdir()
    target.write_bytes(YAML.encode())
    CORE.config_path.unlink()
    CORE.config_path.symlink_to(target)
    CORE.raw_config = yaml_util.load_yaml(CORE.config_path)
    assert _name_edit("garage").path == CORE.config_path


@pytest.mark.skipif(sys.platform == "win32", reason="posix file modes")
def test_write_keeps_the_mode_of_the_file_or_another(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    path.chmod(0o600)
    write_keeping_mode(path, YAML)
    assert path.stat().st_mode & 0o777 == 0o600
    other = tmp_path / "other.yaml"
    write_keeping_mode(other, YAML, like=path)
    assert other.stat().st_mode & 0o777 == 0o600


def test_write_failures_say_which_step_and_why(tmp_path: Path) -> None:
    """A missing mode source, a write that fails, and a mode that cannot be
    put back after the write are three different situations."""
    path = _setup(tmp_path, YAML)
    with pytest.raises(EsphomeError, match="Could not read the mode of .*gone.yaml"):
        write_keeping_mode(path, YAML, like=tmp_path / "gone.yaml")
    with (
        patch("pathlib.Path.chmod", side_effect=OSError("denied")),
        pytest.raises(
            EsphomeError, match="was written but could not get its mode back: denied"
        ),
    ):
        write_keeping_mode(path, YAML)

    def refuse(*_args: object, **_kwargs: object) -> None:
        raise EsphomeError(f"Could not write file at {path}") from OSError("disk full")

    with (
        patch("esphome.yaml_edit.write_file", side_effect=refuse),
        pytest.raises(EsphomeError, match="Could not write file at .*: disk full"),
    ):
        write_keeping_mode(path, YAML)


def test_read_text_reports_a_file_it_cannot_decode(tmp_path: Path) -> None:
    path = tmp_path / "latin1.yaml"
    path.write_bytes(b"caf\xe9: 1\n")
    with pytest.raises(EsphomeError, match="Error reading file"):
        read_text(path)


def test_source_line_refuses_what_is_not_an_editable_file(tmp_path: Path) -> None:
    """A remote package is checked out under the build data and is not the
    user's; a file outside the configuration directory is not either."""
    (tmp_path / ".esphome").mkdir()
    (tmp_path / ".esphome" / "base.yaml").write_bytes(b"name: kitchen\n")
    _setup(tmp_path, "esphome: !include .esphome/base.yaml\n")
    with pytest.raises(EsphomeError, match="not an editable file"):
        source_line(CORE.raw_config[CONF_ESPHOME], CONF_NAME)
    with pytest.raises(EsphomeError, match="was not read from a file"):
        source_line({"name": "kitchen"}, CONF_NAME)
    outside = tmp_path.parent / "outside.yaml"
    outside.write_bytes(b"")
    with pytest.raises(EsphomeError, match="not an editable file"):
        editable_file(outside)
    (tmp_path / "link.yaml").symlink_to(CORE.config_path)
    assert editable_file(tmp_path / "link.yaml") == CORE.config_path.resolve()


def test_insert_after_goes_before_the_rewrite_of_the_same_line(
    tmp_path: Path,
) -> None:
    """Edits apply from the bottom up so an insertion never moves a later
    line; the file's own line ending is used, also on a bare last line."""
    text = YAML.replace("\n", "\r\n").rstrip("\r\n")
    path = _setup(tmp_path, text)
    edit = _name_edit("garage")
    added = LineEdit(path, 4, "  ssid: kitchen", "  password: x", insert_after=True)
    assert rewritten_text(read_text(path), [added, edit]) == (
        text.replace("name: kitchen", "name: garage") + "\r\n  password: x"
    )
    edit.old_line = "changed"
    with pytest.raises(EsphomeError, match="changed since it was read"):
        rewritten_text(read_text(path), [added, edit])


def test_apply_writes_reloads_drops_the_cache_and_restores(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    cache = compiled_config_path(CORE.config_filename)
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text("{}")
    originals = apply_line_edits([_name_edit("garage")])
    assert originals == {path: Snapshot(YAML, YAML.replace("kitchen  #", "garage  #"))}
    assert path.read_text() == YAML.replace("kitchen  #", "garage  #")
    assert not cache.exists()
    cache.write_text("{}")
    restore_files(originals)
    assert path.read_text() == YAML
    assert not cache.exists()


def test_apply_rolls_back_a_rewrite_the_loader_refuses(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    edit = _name_edit("garage")
    edit.new_line = "  name: [unterminated"
    with pytest.raises(EsphomeError, match="no longer loads"):
        apply_line_edits([edit])
    assert path.read_text() == YAML
    with (
        patch("esphome.yaml_util.load_yaml", side_effect=KeyboardInterrupt),
        pytest.raises(KeyboardInterrupt),
    ):
        apply_line_edits([_name_edit("garage")])
    assert path.read_text() == YAML


def test_restore_reports_every_file_it_could_not_write(tmp_path: Path) -> None:
    """A missing file, a failed write and a cache that cannot be dropped are
    all reported; a file the user changed meanwhile is left alone."""
    path = _setup(tmp_path, YAML)
    good = tmp_path / "a.yaml"
    (tmp_path / "b.yaml").write_bytes(b"")
    good.write_bytes(b"")
    with (
        patch("esphome.yaml_edit.write_file", side_effect=[EsphomeError("disk"), None]),
        pytest.raises(EsphomeError, match="Could not restore .*b.yaml: disk"),
    ):
        restore_files({tmp_path / "b.yaml": Snapshot("x", ""), good: Snapshot("y", "")})
    with pytest.raises(
        EsphomeError, match="Could not restore .*gone.yaml: Error reading file"
    ):
        restore_files({tmp_path / "gone.yaml": Snapshot("x", "x")})
    with (
        patch(
            "esphome.compiled_config.invalidate_compiled_config",
            side_effect=EsphomeError("busy"),
        ),
        pytest.raises(EsphomeError, match="Could not restore busy"),
    ):
        restore_files({path: Snapshot(YAML, YAML)})
    originals = apply_line_edits([_name_edit("garage")])
    edited = path.read_text() + "logger:\n"
    path.write_bytes(edited.encode())
    with pytest.raises(EsphomeError, match="changed since it was written, left as is"):
        restore_files(originals)
    assert path.read_text() == edited


def test_apply_rolls_back_when_the_cache_cannot_be_dropped(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    with (
        patch(
            "esphome.compiled_config.invalidate_compiled_config",
            side_effect=[EsphomeError("busy"), None],
        ),
        pytest.raises(EsphomeError, match="busy"),
    ):
        apply_line_edits([_name_edit("garage")])
    assert path.read_text() == YAML


def test_apply_reports_a_rollback_that_also_failed(tmp_path: Path) -> None:
    """The rewrite lands, the reload fails, and the rollback write fails too."""
    from esphome.yaml_edit import write_file

    _setup(tmp_path, YAML)
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
        apply_line_edits([_name_edit("garage")])


def test_a_failed_rollback_keeps_an_interrupt_an_interrupt(tmp_path: Path) -> None:
    _setup(tmp_path, YAML)
    with (
        patch("esphome.yaml_util.load_yaml", side_effect=KeyboardInterrupt),
        patch("pathlib.Path.chmod", side_effect=[None, OSError("disk")]),
        pytest.raises(KeyboardInterrupt) as info,
    ):
        apply_line_edits([_name_edit("garage")])
    assert "disk" in "".join(info.value.__notes__)


def test_rollback_after_a_failed_second_write_leaves_the_untouched_file_alone(
    tmp_path: Path,
) -> None:
    """The file that was never written is not reported as changed."""
    path = _setup(tmp_path, YAML)
    other = tmp_path / "other.yaml"
    other.write_bytes(b"x: 1\n")
    edits = [_name_edit("garage"), LineEdit(other, 0, "x: 1", "x: 2")]
    with (
        patch(
            "esphome.yaml_edit.write_keeping_mode",
            side_effect=[None, EsphomeError("disk")],
        ),
        pytest.raises(EsphomeError) as info,
    ):
        apply_line_edits(edits)
    assert "changed since it was written" not in str(info.value)
    assert path.read_text() == YAML and other.read_text() == "x: 1\n"


def test_restore_goes_on_after_an_interrupt(tmp_path: Path) -> None:
    """A second Ctrl-C must not leave the other files holding the new value."""
    path = _setup(tmp_path, YAML)
    other = tmp_path / "other.yaml"
    other.write_bytes(b"x: 1\n")
    calls: list[Path] = []

    def interrupt_at(n: int) -> Any:
        def write(target: Path, text: str) -> None:
            calls.append(target)
            if len(calls) == n:
                raise KeyboardInterrupt
            write_keeping_mode(target, text)

        return write

    originals = apply_line_edits(
        [_name_edit("garage"), LineEdit(other, 0, "x: 1", "x: 2")]
    )
    with (
        patch("esphome.yaml_edit.write_keeping_mode", side_effect=interrupt_at(1)),
        pytest.raises(EsphomeError, match="Could not restore .*: interrupted"),
    ):
        restore_files(originals)
    assert calls == [path, other] and other.read_text() == "x: 1\n"
    # Inside apply the failed rollback is reported with the reason it ran
    _setup(tmp_path, YAML)
    calls.clear()
    with (
        patch("esphome.yaml_util.load_yaml", side_effect=EsphomeError("broken")),
        patch("esphome.yaml_edit.write_keeping_mode", side_effect=interrupt_at(2)),
        pytest.raises(RestoreError, match="broken; Could not restore .*interrupted"),
    ):
        apply_line_edits([_name_edit("garage")])
