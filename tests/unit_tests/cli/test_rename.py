"""Tests for ``esphome rename``."""

from __future__ import annotations

from collections.abc import Generator
from dataclasses import dataclass
from pathlib import Path
import sys
from typing import Any
from unittest.mock import Mock, patch

import pytest
from pytest import CaptureFixture

from esphome.cli.rename import command_rename
from esphome.const import CONF_ESPHOME, CONF_NAME, CONF_SUBSTITUTIONS
from esphome.core import CORE


@dataclass
class MockArgs:
    name: str | None = None
    dashboard: bool = False


def setup_core(tmp_path: Path, config: dict[str, Any] | None = None) -> None:
    """Point CORE at a config in ``tmp_path``; the tests override the path."""
    CORE.config = config or {}
    CORE.config_path = tmp_path / "test.yaml"
    CORE.name = "test"


@pytest.fixture
def mock_run_external_process() -> Generator[Mock]:
    """The child esphome the command starts to validate and install."""
    with patch("esphome.cli.rename.run_external_process") as mock:
        mock.return_value = 0
        yield mock


def test_command_rename_invalid_characters(
    tmp_path: Path, capfd: CaptureFixture[str]
) -> None:
    """Test command_rename with invalid characters in name."""
    setup_core(tmp_path=tmp_path)

    # Test with invalid character (space)
    args = MockArgs(name="invalid name")
    result = command_rename(args, {})

    assert result == 1
    captured = capfd.readouterr()
    assert "invalid character" in captured.out.lower()


def test_command_rename_complex_yaml(
    tmp_path: Path, capfd: CaptureFixture[str]
) -> None:
    """Test command_rename with complex YAML that cannot be renamed."""
    config_file = tmp_path / "test.yaml"
    config_file.write_text("# Complex YAML without esphome section\nsome_key: value\n")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file

    args = MockArgs(name="newname")
    result = command_rename(args, {})

    assert result == 1
    captured = capfd.readouterr()
    assert "complex yaml" in captured.out.lower()


def test_command_rename_success(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test successful rename of a simple configuration."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
esphome:
  name: oldname

esp32:
  board: nodemcu-32s

wifi:
  ssid: "test"
  password: "test1234"
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file

    # Set up CORE.config to avoid ValueError when accessing CORE.address
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}

    args = MockArgs(name="newname", dashboard=False)

    # Simulate successful validation and upload
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0

    # Verify new file was created
    new_file = tmp_path / "newname.yaml"
    assert new_file.exists()

    # Verify old file was removed
    assert not config_file.exists()

    # Verify content was updated
    content = new_file.read_text()
    assert (
        'name: "newname"' in content
        or "name: 'newname'" in content
        or "name: newname" in content
    )

    captured = capfd.readouterr()
    assert "SUCCESS" in captured.out


def test_command_rename_with_substitutions(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Test rename with substitutions in YAML."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
substitutions:
  device_name: oldname

esphome:
  name: ${device_name}

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file

    # Set up CORE.config to avoid ValueError when accessing CORE.address
    CORE.config = {
        CONF_ESPHOME: {CONF_NAME: "oldname"},
        CONF_SUBSTITUTIONS: {"device_name": "oldname"},
    }

    args = MockArgs(name="newname", dashboard=False)

    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0

    # Verify substitution was updated
    new_file = tmp_path / "newname.yaml"
    content = new_file.read_text()
    assert 'device_name: "newname"' in content


def test_command_rename_validation_failure(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename when validation fails."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
esphome:
  name: oldname

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file

    args = MockArgs(name="newname", dashboard=False)

    # First call for validation fails
    mock_run_external_process.return_value = 1

    result = command_rename(args, {})

    assert result == 1

    # Verify new file was created but then removed due to failure
    new_file = tmp_path / "newname.yaml"
    assert not new_file.exists()

    # Verify old file still exists (not removed on failure)
    assert config_file.exists()

    captured = capfd.readouterr()
    assert "Rename failed" in captured.out


def test_command_rename_install_failure_reverts(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename when the install (esphome run) step fails."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
esphome:
  name: oldname

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}

    args = MockArgs(name="newname", dashboard=False)

    # First call (config validation) succeeds; second (esphome run) fails.
    mock_run_external_process.side_effect = [0, 1]

    result = command_rename(args, {})

    assert result == 1

    # New file was unlinked when install failed.
    new_file = tmp_path / "newname.yaml"
    assert not new_file.exists()

    # Old file is preserved so the device stays reachable under the
    # original hostname.
    assert config_file.exists()


def test_command_rename_target_exists_refuses(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename refuses when the target filename already exists.

    Without this guard, the rename would overwrite the unrelated
    device's YAML and OTA-install our firmware to the wrong device.
    """
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
esphome:
  name: oldname

esp32:
  board: nodemcu-32s
""")
    target_file = tmp_path / "newname.yaml"
    target_file.write_text("""
esphome:
  name: someoneelse

esp32:
  board: nodemcu-32s
""")
    target_original = target_file.read_text()
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}

    args = MockArgs(name="newname", dashboard=False)

    result = command_rename(args, {})

    assert result == 1
    # No subprocess work happened — refusal is up-front.
    mock_run_external_process.assert_not_called()
    # Target file untouched: same content, still on disk.
    assert target_file.exists()
    assert target_file.read_text() == target_original
    # Source file untouched.
    assert config_file.exists()

    captured = capfd.readouterr()
    assert "already exists" in captured.out


def test_command_rename_same_name_refuses(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename refuses when the new name matches the current name.

    A same-name rename would otherwise re-write the YAML and queue
    a redundant compile + install — wasted work the user almost
    certainly didn't intend.
    """
    config_file = tmp_path / "samename.yaml"
    config_file.write_text("""
esphome:
  name: samename

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "samename"}}

    args = MockArgs(name="samename", dashboard=False)

    result = command_rename(args, {})

    assert result == 1
    mock_run_external_process.assert_not_called()
    # File preserved verbatim — no rewrite happened.
    assert config_file.exists()

    captured = capfd.readouterr()
    assert "already" in captured.out.lower()


def test_command_rename_does_not_touch_friendly_name_substring(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    r"""Test rename does not match the ``name:`` substring of ``friendly_name:``.

    Without anchoring the regex at line start, the pattern
    ``\s*name:\s+<old>`` could match the trailing ``name:``
    substring inside ``friendly_name: <old>``. The rewrite would
    flip both lines to the new name, leaving the user with a
    silently corrupted ``friendly_name``.
    """
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
esphome:
  name: oldname
  friendly_name: oldname

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}

    args = MockArgs(name="newname", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0
    new_file = tmp_path / "newname.yaml"
    content = new_file.read_text()
    # esphome.name swapped.
    assert 'name: "newname"' in content
    # friendly_name kept verbatim.
    assert "friendly_name: oldname" in content


def test_command_rename_does_not_match_old_name_as_value_prefix(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    r"""Test rename does not match ``old_name`` as a prefix of a longer value.

    With ``old_name = kitchen`` the value ``kitchen2`` (a sensor
    or wifi entry) would otherwise match the unanchored
    ``["']?kitchen["']?`` pattern at the prefix and get
    rewritten to the new name. The end-of-value lookahead keeps
    the match restricted to whole tokens.
    """
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: kitchen

esp32:
  board: nodemcu-32s

wifi:
  ap:
    ssid: kitchen2
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="garage", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0
    new_file = tmp_path / "garage.yaml"
    content = new_file.read_text()
    assert 'name: "garage"' in content
    # The wifi ssid value is unrelated and stays intact.
    assert "ssid: kitchen2" in content


def test_command_rename_same_resolved_name_refuses(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename refuses when ``new_name`` matches the resolved device name.

    The path-equality check only catches the case where the
    config filename matches the device name. For a config whose
    filename and ``esphome.name`` differ (here ``weird-file.yaml``
    holds ``esphome.name: kitchen``), running
    ``esphome rename weird-file.yaml kitchen`` would otherwise
    fall through to the rewrite + install: the YAML's name stays
    ``kitchen``, the file is renamed to ``kitchen.yaml``, and the
    device gets a redundant flash. Refuse up-front so the
    "already the device's name" message matches reality.
    """
    config_file = tmp_path / "weird-file.yaml"
    config_file.write_text("""
esphome:
  name: kitchen

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="kitchen", dashboard=False)

    result = command_rename(args, {})

    assert result == 1
    mock_run_external_process.assert_not_called()
    # Source file untouched, no derived target written.
    assert config_file.exists()
    assert not (tmp_path / "kitchen.yaml").exists()

    captured = capfd.readouterr()
    assert "already" in captured.out.lower()


def test_command_rename_target_path_equals_source_refuses(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename refuses when the new path resolves to the source file.

    Reachable only when the YAML's filename and ``esphome.name``
    disagree — here ``kitchen.yaml`` holds ``esphome.name: garage``
    and the user runs ``esphome rename kitchen.yaml kitchen``. The
    name-equality check above passes (``garage != kitchen``), but
    ``<config_dir>/kitchen.yaml`` resolves to the source file
    itself, so the rewrite would clobber the source mid-rename.
    Refuse rather than silently overwriting.
    """
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: garage

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "garage"}}

    args = MockArgs(name="kitchen", dashboard=False)

    result = command_rename(args, {})

    assert result == 1
    mock_run_external_process.assert_not_called()
    # Source file still present and unmodified.
    assert config_file.exists()
    assert "name: garage" in config_file.read_text()

    captured = capfd.readouterr()
    assert "already" in captured.out.lower()


def test_command_rename_does_not_touch_lookalike_name_in_other_blocks(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Test rename only swaps the esphome.name line.

    A device whose name happens to match a sensor's / output's
    ``name:`` value must not have those other names rewritten —
    they're independent. Without an anchor for the esphome block
    a naive regex would clobber every line whose value matches.
    """
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: kitchen

esp32:
  board: nodemcu-32s

sensor:
  - platform: template
    name: kitchen
    lambda: 'return 0;'
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="garage", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0

    new_file = tmp_path / "garage.yaml"
    content = new_file.read_text()
    # esphome.name renamed.
    assert 'name: "garage"' in content
    # Sensor's name is the user's entity name — must not be touched.
    assert "    name: kitchen\n" in content


def test_command_rename_preserves_trailing_comment(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Test rename preserves a trailing ``# comment`` on the name line."""
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: kitchen  # primary device

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="garage", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0

    new_file = tmp_path / "garage.yaml"
    content = new_file.read_text()
    assert "# primary device" in content


def test_command_rename_handles_double_quoted_value(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Test rename matches when the existing value is double-quoted."""
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: "kitchen"

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="garage", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0
    new_file = tmp_path / "garage.yaml"
    assert 'name: "garage"' in new_file.read_text()


def test_command_rename_handles_single_quoted_value(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Test rename matches when the existing value is single-quoted."""
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: 'kitchen'

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="garage", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0
    new_file = tmp_path / "garage.yaml"
    assert 'name: "garage"' in new_file.read_text()


def test_command_rename_leaves_a_lookalike_substitution_line_alone(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Only the substitution's own line changes; another block's field of
    the same name and value is not it."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
substitutions:
  device_name: oldname

esphome:
  name: ${device_name}

example:
  device_name: oldname

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {
        CONF_ESPHOME: {CONF_NAME: "oldname"},
        CONF_SUBSTITUTIONS: {"device_name": "oldname"},
    }
    assert command_rename(MockArgs(name="newname", dashboard=False), {}) == 0
    content = (tmp_path / "newname.yaml").read_text()
    assert 'device_name: "newname"' in content
    assert "example:\n  device_name: oldname\n" in content


def test_command_rename_keeps_line_endings_and_mode(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """A CRLF file stays CRLF and the new file gets the old one's mode."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_bytes(
        b"esphome:\r\n  name: oldname  # device\r\n\r\nesp32:\r\n  board: nodemcu-32s\r\n"
    )
    if sys.platform != "win32":
        config_file.chmod(0o600)
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}
    assert command_rename(MockArgs(name="newname", dashboard=False), {}) == 0
    new_file = tmp_path / "newname.yaml"
    assert new_file.read_bytes() == (
        b'esphome:\r\n  name: "newname"  # device\r\n\r\nesp32:\r\n  board: nodemcu-32s\r\n'
    )
    if sys.platform != "win32":
        assert new_file.stat().st_mode & 0o777 == 0o600


@pytest.mark.parametrize(
    ("yaml_text", "extra"),
    [
        ("esphome:\n  name: ${missing}\n", {}),
        ("esphome: {name: oldname}\n", {}),
        ("esphome: !include base.yaml\n", {"base.yaml": "name: oldname\n"}),
        (
            (
                "named: &named\n  name: oldname\n\nesphome:\n  <<: *named\n\n"
                "sensor:\n  - platform: template\n    <<: *named\n"
            ),
            {},
        ),
    ],
    ids=["missing_substitution", "flow_mapping", "included_name", "merged_name"],
)
def test_command_rename_refuses_shapes_without_a_plain_name_line(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
    yaml_text: str,
    extra: dict[str, str],
) -> None:
    """The name line must be a plain value in the file being renamed."""
    for name, text in extra.items():
        (tmp_path / name).write_text(text)
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text(yaml_text)
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}
    assert command_rename(MockArgs(name="newname", dashboard=False), {}) == 1
    mock_run_external_process.assert_not_called()
    assert "complex yaml" in capfd.readouterr().out.lower()


def test_command_rename_removes_the_new_file_when_its_mode_cannot_be_set(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """No orphan is left for the next attempt to trip over."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("esphome:\n  name: oldname\n")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}
    with patch("pathlib.Path.chmod", side_effect=OSError("read-only share")):
        assert command_rename(MockArgs(name="newname", dashboard=False), {}) == 1
    assert not (tmp_path / "newname.yaml").exists()
    mock_run_external_process.assert_not_called()
    assert "Rename failed" in capfd.readouterr().out


def test_command_rename_refuses_a_name_without_a_source_line(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """A key the loader did not read from a file cannot be located."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("esphome:\n  name: oldname\n")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}
    with patch("esphome.yaml_edit.source_of", return_value=None):
        assert command_rename(MockArgs(name="newname", dashboard=False), {}) == 1
    mock_run_external_process.assert_not_called()
    assert "was not read from" in capfd.readouterr().out


def test_command_rename_passes_dashboard_to_the_install(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("esphome:\n  name: oldname\n")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}
    assert command_rename(MockArgs(name="newname", dashboard=True), {}) == 0
    install = mock_run_external_process.call_args_list[-1].args
    assert install[-6:-4] == ("--dashboard", "run")


def test_command_rename_interrupted_install_reverts(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("esphome:\n  name: oldname\n")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}
    mock_run_external_process.side_effect = [0, KeyboardInterrupt]
    assert command_rename(MockArgs(name="newname", dashboard=False), {}) == 1
    assert not (tmp_path / "newname.yaml").exists()
    assert config_file.exists()


def test_command_rename_reads_a_config_linked_from_outside(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """The source is only read; the new file lands in the config directory."""
    outside = tmp_path / "elsewhere.yaml"
    outside.write_text("esphome:\n  name: oldname\n")
    config_dir = tmp_path / "config"
    config_dir.mkdir()
    config_file = config_dir / "oldname.yaml"
    config_file.symlink_to(outside)
    setup_core(tmp_path=config_dir)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}
    assert command_rename(MockArgs(name="newname", dashboard=False), {}) == 0
    assert (config_dir / "newname.yaml").read_text() == 'esphome:\n  name: "newname"\n'
    assert not config_file.exists()
    assert outside.exists()


def test_command_rename_reports_an_orphan_it_could_not_remove(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """The write failure is the message; a cleanup failure is added to it."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("esphome:\n  name: oldname\n")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}
    with (
        patch("pathlib.Path.chmod", side_effect=OSError("read-only share")),
        patch("pathlib.Path.unlink", side_effect=OSError("busy")),
    ):
        assert command_rename(MockArgs(name="newname", dashboard=False), {}) == 1
    out = capfd.readouterr().out
    assert "Rename failed" in out
    assert "Could not remove" in out and "newname.yaml" in out
