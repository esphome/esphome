"""Tests for the Zephyr framework installer."""

from pathlib import Path
from unittest.mock import patch

from esphome.components.zephyr import framework


def test_check_and_install_builds_the_venv(tmp_path: Path) -> None:
    """Each Zephyr version gets its own venv, built from zephyr/requirements.txt
    under that version's install lock."""
    with (
        patch.object(framework, "get_zephyr_tools_path", return_value=tmp_path),
        patch.object(framework, "ensure_venv") as ensure,
        patch("filelock.FileLock") as file_lock,
    ):
        framework.check_and_install("v4.5.0")

    ensure.assert_called_once_with(
        tmp_path / "penvs" / "v4.5.0",
        Path(framework.__file__).parent / "requirements.txt",
        "v4.5.0",
    )
    assert framework._REQUIREMENTS.is_file()
    assert [Path(c.args[0]).name for c in file_lock.call_args_list] == [
        "penv-v4.5.0.lock"
    ]
