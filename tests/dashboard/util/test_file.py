import os
from pathlib import Path
from unittest.mock import patch

import py
import pytest

from esphome.dashboard.util.file import write_file, write_utf8_file


def test_write_utf8_file(tmp_path: Path) -> None:
    write_utf8_file(tmp_path.joinpath("foo.txt"), "foo")
    assert tmp_path.joinpath("foo.txt").read_text() == "foo"

    with pytest.raises(OSError):
        write_utf8_file(Path("/not-writable"), "bar")


def test_write_file(tmp_path: Path) -> None:
    write_file(tmp_path.joinpath("foo.txt"), b"foo")
    assert tmp_path.joinpath("foo.txt").read_text() == "foo"


def test_write_utf8_file_fails_at_rename(
    tmpdir: py.path.local, caplog: pytest.LogCaptureFixture
) -> None:
    """Test that if rename fails not not remove, we do not log the failed cleanup."""
    test_dir = tmpdir.mkdir("files")
    test_file = Path(test_dir / "test.json")

    with pytest.raises(OSError), patch(
        "esphome.dashboard.util.file.os.replace", side_effect=OSError
    ):
        write_utf8_file(test_file, '{"some":"data"}', False)

    assert not os.path.exists(test_file)

    assert "File replacement cleanup failed" not in caplog.text


def test_write_utf8_file_fails_at_rename_and_remove(
    tmpdir: py.path.local, caplog: pytest.LogCaptureFixture
) -> None:
    """Test that if rename and remove both fail, we log the failed cleanup."""
    test_dir = tmpdir.mkdir("files")
    test_file = Path(test_dir / "test.json")

    with pytest.raises(OSError), patch(
        "esphome.dashboard.util.file.os.remove", side_effect=OSError
    ), patch("esphome.dashboard.util.file.os.replace", side_effect=OSError):
        write_utf8_file(test_file, '{"some":"data"}', False)

    assert "File replacement cleanup failed" in caplog.text
