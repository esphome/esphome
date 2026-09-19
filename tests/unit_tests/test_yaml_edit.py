"""Tests for rewriting single lines of a yaml file."""

from __future__ import annotations

from pathlib import Path
import sys

import pytest

from esphome import yaml_util
from esphome.config import do_substitution_pass
from esphome.const import CONF_ESPHOME, CONF_NAME
from esphome.core import CORE, EsphomeError
from esphome.yaml_edit import (
    LineEdit,
    field_line_re,
    line_at,
    read_text,
    rewrite,
    rewritten_text,
    source_line,
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
    doc, line_no, text = source_line(CORE.raw_config[CONF_ESPHOME], CONF_NAME)
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


def test_source_line_resolves_a_symlink(tmp_path: Path) -> None:
    target = tmp_path / "shared" / "test.yaml"
    target.parent.mkdir()
    target.write_bytes(YAML.encode())
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    CORE.config_path.symlink_to(target)
    CORE.raw_config = yaml_util.load_yaml(CORE.config_path)
    assert _name_edit("garage").path == target.resolve()


@pytest.mark.skipif(sys.platform == "win32", reason="posix file modes")
def test_write_keeps_the_mode_of_the_file_or_another(tmp_path: Path) -> None:
    path = _setup(tmp_path, YAML)
    path.chmod(0o600)
    write_keeping_mode(path, YAML)
    assert path.stat().st_mode & 0o777 == 0o600
    other = tmp_path / "other.yaml"
    write_keeping_mode(other, YAML, like=path)
    assert other.stat().st_mode & 0o777 == 0o600


def test_mode_failure_is_reported(tmp_path: Path) -> None:
    from unittest.mock import patch

    path = _setup(tmp_path, YAML)
    with (
        patch("pathlib.Path.chmod", side_effect=OSError("denied")),
        pytest.raises(EsphomeError, match="Could not keep the mode"),
    ):
        write_keeping_mode(path, YAML)


def test_read_text_reports_a_file_it_cannot_decode(tmp_path: Path) -> None:
    path = tmp_path / "latin1.yaml"
    path.write_bytes(b"caf\xe9: 1\n")
    with pytest.raises(EsphomeError, match="Error reading file"):
        read_text(path)
