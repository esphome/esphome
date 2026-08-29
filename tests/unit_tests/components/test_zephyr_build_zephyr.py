"""Unit tests for esphome.components.zephyr.build_zephyr."""

from __future__ import annotations

from pathlib import Path
import subprocess
from unittest.mock import patch

from esphome.components.zephyr.build_zephyr import (
    _runner_supports_dev_id,
    resolve_dev_id,
)

# ---------------------------------------------------------------------------
# _runner_supports_dev_id
# ---------------------------------------------------------------------------


def test_runner_supports_dev_id_reads_capability_from_subprocess_stdout(
    tmp_path: Path,
) -> None:
    def fake_run(cmd, **kwargs):
        return subprocess.CompletedProcess(cmd, 0, stdout="True\n", stderr="")

    with patch("subprocess.run", side_effect=fake_run):
        assert _runner_supports_dev_id(Path("python"), tmp_path, "jlink") is True


def test_runner_supports_dev_id_false_for_capability_false(tmp_path: Path) -> None:
    def fake_run(cmd, **kwargs):
        return subprocess.CompletedProcess(cmd, 0, stdout="False\n", stderr="")

    with patch("subprocess.run", side_effect=fake_run):
        assert _runner_supports_dev_id(Path("python"), tmp_path, "openocd") is False


def test_runner_supports_dev_id_false_for_unknown_runner(tmp_path: Path) -> None:
    """The query script itself prints False on any exception (e.g. an unknown
    runner name raising ValueError inside runners.get_runner_cls()) -- this
    doesn't need a real SDK checkout to verify, just that stdout "False" is
    treated as "don't forward -i", same as any other false case.
    """

    def fake_run(cmd, **kwargs):
        return subprocess.CompletedProcess(cmd, 0, stdout="False\n", stderr="")

    with patch("subprocess.run", side_effect=fake_run):
        assert (
            _runner_supports_dev_id(Path("python"), tmp_path, "not_a_real_runner")
            is False
        )


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
