"""Unit tests for esphome.components.zephyr.build_zephyr."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
from typing import Any
from unittest.mock import patch

import pytest

from esphome.components.zephyr.build_zephyr import (
    _runner_supports_dev_id,
    resolve_dev_id,
    run_west_blobs_fetch,
)

# ---------------------------------------------------------------------------
# _runner_supports_dev_id
# ---------------------------------------------------------------------------


@pytest.fixture
def fake_west_runners(tmp_path: Path) -> Path:
    """A framework tree whose `runners` package answers like west's, so the real
    query script runs end to end: jlink has dev_id, openocd does not, any other
    name raises."""
    package = tmp_path / "zephyr" / "scripts" / "west_commands" / "runners"
    package.mkdir(parents=True)
    (package / "__init__.py").write_text(
        "from types import SimpleNamespace\n"
        "_DEV_ID = {'jlink': True, 'openocd': False}\n"
        "def get_runner_cls(name):\n"
        "    if name not in _DEV_ID:\n"
        "        raise ValueError(name)\n"
        "    caps = SimpleNamespace(dev_id=_DEV_ID[name])\n"
        "    return SimpleNamespace(capabilities=lambda: caps)\n"
    )
    return tmp_path


@pytest.mark.parametrize(
    ("runner", "expected"),
    [("jlink", True), ("openocd", False), ("not_a_real_runner", False)],
)
def test_runner_supports_dev_id_reads_the_runners_own_capability(
    fake_west_runners: Path, runner: str, expected: bool
) -> None:
    assert (
        _runner_supports_dev_id(Path(sys.executable), fake_west_runners, runner)
        is expected
    )


def test_runner_supports_dev_id_false_when_runners_package_missing(
    tmp_path: Path,
) -> None:
    assert _runner_supports_dev_id(Path(sys.executable), tmp_path, "jlink") is False


def test_runner_supports_dev_id_queries_the_given_runner_in_the_west_tree(
    tmp_path: Path,
) -> None:
    with patch("subprocess.run") as mock_run:
        mock_run.return_value = subprocess.CompletedProcess([], 0, stdout="True\n")
        _runner_supports_dev_id(Path("python"), tmp_path, "jlink")

    cmd = mock_run.call_args.args[0]
    assert cmd[:2] == ["python", "-c"]
    assert "get_runner_cls('jlink')" in cmd[2]
    assert str(tmp_path / "zephyr" / "scripts" / "west_commands") in cmd[2]
    assert mock_run.call_args.kwargs["timeout"] == 10


def test_runner_supports_dev_id_false_when_subprocess_unavailable(
    tmp_path: Path,
) -> None:
    with patch("subprocess.run", side_effect=OSError("no such file")):
        assert _runner_supports_dev_id(Path("python"), tmp_path, "jlink") is False


def test_runner_supports_dev_id_false_on_timeout(tmp_path: Path) -> None:
    with patch(
        "subprocess.run",
        side_effect=subprocess.TimeoutExpired(cmd=["python"], timeout=10),
    ):
        assert _runner_supports_dev_id(Path("python"), tmp_path, "jlink") is False


# ---------------------------------------------------------------------------
# resolve_dev_id
# ---------------------------------------------------------------------------


def test_resolve_dev_id_returns_none_when_runner_lacks_dev_id(tmp_path: Path) -> None:
    build_dir = tmp_path / "build"
    (build_dir / "zephyr").mkdir(parents=True)
    (build_dir / "zephyr" / "runners.yaml").write_text("flash-runner: uf2\n")

    with patch(
        "esphome.components.zephyr.build_zephyr._runner_supports_dev_id",
        return_value=False,
    ):
        assert (
            resolve_dev_id(Path("python"), tmp_path, build_dir, "/dev/ttyACM0") is None
        )


def test_resolve_dev_id_returns_serial_number_when_runner_supports_dev_id(
    tmp_path: Path,
) -> None:
    build_dir = tmp_path / "build"
    (build_dir / "zephyr").mkdir(parents=True)
    (build_dir / "zephyr" / "runners.yaml").write_text("flash-runner: jlink\n")

    with (
        patch(
            "esphome.components.zephyr.build_zephyr._runner_supports_dev_id",
            return_value=True,
        ) as mock_supports,
        patch(
            "esphome.components.zephyr.build_zephyr.get_serial_number",
            return_value="ABC123",
        ),
    ):
        result = resolve_dev_id(Path("python"), tmp_path, build_dir, "/dev/ttyACM0")

    assert result == "ABC123"
    mock_supports.assert_called_once_with(Path("python"), tmp_path, "jlink")


def test_resolve_dev_id_returns_none_when_runners_yaml_missing(
    tmp_path: Path,
) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    assert resolve_dev_id(Path("python"), tmp_path, build_dir, "/dev/ttyACM0") is None


# ---------------------------------------------------------------------------
# run_west_blobs_fetch -- fetch once per module revision
# ---------------------------------------------------------------------------

_SENTINEL = ".blobs_hal_espressif_ready"


def _fetch_blobs(tmp_path: Path, revision: str | None) -> list[list[str]]:
    """Run run_west_blobs_fetch with `west list` reporting `revision` (None = west
    fails); return the `west blobs fetch` commands it ran."""

    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        if revision is None:
            return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="boom")
        return subprocess.CompletedProcess(cmd, 0, stdout=f"{revision}\n", stderr="")

    fetches: list[list[str]] = []
    with (
        patch("subprocess.run", side_effect=fake_run),
        patch(
            "esphome.components.zephyr.build_zephyr.run_command_ok",
            side_effect=lambda cmd, **kw: fetches.append(cmd) or True,
        ),
    ):
        run_west_blobs_fetch(
            Path("python"), tmp_path, {}, "hal_espressif", ".*", _SENTINEL
        )
    return fetches


def test_blobs_fetched_and_revision_recorded_first_time(tmp_path: Path) -> None:
    assert len(_fetch_blobs(tmp_path, "a" * 40)) == 1
    assert (tmp_path / _SENTINEL).read_text() == "a" * 40


def test_blobs_skipped_when_revision_unchanged(tmp_path: Path) -> None:
    (tmp_path / _SENTINEL).write_text("a" * 40)
    assert _fetch_blobs(tmp_path, "a" * 40) == []


def test_blobs_refetched_when_module_revision_moved(tmp_path: Path) -> None:
    (tmp_path / _SENTINEL).write_text("a" * 40)
    assert len(_fetch_blobs(tmp_path, "b" * 40)) == 1
    assert (tmp_path / _SENTINEL).read_text() == "b" * 40


def test_blobs_refetched_for_legacy_empty_sentinel(tmp_path: Path) -> None:
    (tmp_path / _SENTINEL).touch()
    assert len(_fetch_blobs(tmp_path, "a" * 40)) == 1


def test_blobs_always_fetched_when_revision_unknown(tmp_path: Path) -> None:
    (tmp_path / _SENTINEL).write_text("a" * 40)
    assert len(_fetch_blobs(tmp_path, None)) == 1
