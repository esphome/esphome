"""Unit tests for esphome.components.zephyr.build_zephyr."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
from typing import Any
from unittest.mock import patch

import pytest

from esphome.build_helpers.tools_cache import SDK_ZEPHYR_TOOLS_CACHE, tools_cache_path
from esphome.components.zephyr.build_zephyr import (
    _app_domain_args,
    _ccache_env,
    _imgtool_sign_commands,
    _runner_supports_dev_id,
    check_bootloader_built,
    resign_direct_xip_images,
    resolve_dev_id,
    run_west_blobs_fetch,
)
from esphome.core import CORE, EsphomeError

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


# ---------------------------------------------------------------------------
# direct-xip re-signing
# ---------------------------------------------------------------------------

_SIGN = (
    "/penv/bin/python /mcuboot/scripts/imgtool.py sign --version 0.0.0+0 "
    '--header-size 0x20 --slot-size 1376256 --align 4 --key "/mcuboot/root$ ec.pem" '
    "{extra}/b/zephyr.bin /b/{out}"
)
_NINJA = (
    "build zephyr/zephyr.elf: LINK\n"
    "  POST_BUILD = cd /b && /penv/bin/python check.py --elf-file=/b/zephyr.elf && "
    + _SIGN.format(extra="", out="zephyr.signed.bin")
    + " && "
    + _SIGN.format(extra="--pad --confirm ", out="zephyr.signed.confirmed.bin")
    + "\n"
)


def _write_ninja(build_dir: Path, text: str = _NINJA) -> None:
    for image_dir in ("zephyr", "zephyr_slot1_variant"):
        (build_dir / image_dir).mkdir(parents=True)
        (build_dir / image_dir / "build.ninja").write_text(text)


def test_imgtool_sign_commands_finds_every_signed_output() -> None:
    commands = _imgtool_sign_commands(_NINJA)

    assert [c[-1] for c in commands] == [
        "/b/zephyr.signed.bin",
        "/b/zephyr.signed.confirmed.bin",
    ]
    assert commands[0][:3] == [
        "/penv/bin/python",
        "/mcuboot/scripts/imgtool.py",
        "sign",
    ]
    assert "--confirm" in commands[1]
    # Ninja's "$ " escape inside the shell quotes is a plain space
    assert "/mcuboot/root ec.pem" in commands[0]


def test_resign_uses_one_new_version_for_every_image(tmp_path: Path) -> None:
    _write_ninja(tmp_path)
    with (
        patch("esphome.components.zephyr.build_zephyr.time.time", return_value=1234),
        patch("esphome.components.zephyr.build_zephyr.subprocess.run") as run,
    ):
        run.return_value.returncode = 0
        resign_direct_xip_images(tmp_path)

    calls = [c.args[0] for c in run.call_args_list]
    assert len(calls) == 4
    assert {c[c.index("--version") + 1] for c in calls} == {"0.0.0+1234"}
    assert {c.kwargs["cwd"] for c in run.call_args_list} == {
        tmp_path / "zephyr",
        tmp_path / "zephyr_slot1_variant",
    }


def test_resign_fails_when_no_sign_command_found(tmp_path: Path) -> None:
    _write_ninja(tmp_path, "build zephyr/zephyr.elf: LINK\n")

    with pytest.raises(EsphomeError, match="No imgtool sign command"):
        resign_direct_xip_images(tmp_path)


def test_resign_fails_when_signing_fails(tmp_path: Path) -> None:
    _write_ninja(tmp_path)
    with patch("esphome.components.zephyr.build_zephyr.subprocess.run") as run:
        run.return_value.returncode = 1
        with pytest.raises(EsphomeError, match="Signing"):
            resign_direct_xip_images(tmp_path)


# ---------------------------------------------------------------------------
# _ccache_env
# ---------------------------------------------------------------------------

_CCACHE_KEYS = (
    "CCACHE_DIR",
    "CCACHE_DEPEND",
    "CCACHE_NOHASHDIR",
    "CCACHE_BASEDIR",
    "CCACHE_DISABLE",
    "CCACHE_IGNOREOPTIONS",
)


@pytest.fixture
def clean_ccache_env(monkeypatch: pytest.MonkeyPatch) -> None:
    for key in _CCACHE_KEYS:
        monkeypatch.delenv(key, raising=False)


@pytest.mark.usefixtures("clean_ccache_env")
def test_ccache_env_disabled_turns_zephyr_ccache_off(tmp_path: Path) -> None:
    """Without a resolved ccache, Zephyr's self-enabled ccache must not cache."""
    with patch("esphome.build_helpers.ccache.resolve_ccache_path", return_value=None):
        assert _ccache_env({}, tmp_path / "zephyr") == {"CCACHE_DISABLE": "1"}
        # A caller-supplied value is left alone
        assert _ccache_env({"CCACHE_DISABLE": "0"}, tmp_path / "zephyr") == {}


@pytest.mark.usefixtures("clean_ccache_env")
def test_ccache_env_enabled_uses_managed_settings(tmp_path: Path) -> None:
    """A resolved ccache gets the shared settings and the sdk-zephyr cache dir."""
    CORE.build_path = tmp_path / "build"
    source_dir = tmp_path / "build" / "zephyr"
    with patch(
        "esphome.build_helpers.ccache.resolve_ccache_path",
        return_value="/usr/bin/ccache",
    ):
        env = _ccache_env({}, source_dir)
    assert env["CCACHE_DIR"] == str(
        tools_cache_path(*SDK_ZEPHYR_TOOLS_CACHE) / "ccache"
    )
    assert env["CCACHE_DEPEND"] == "1"
    assert env["CCACHE_BASEDIR"] == str((tmp_path / "build").resolve())
    assert "CCACHE_DISABLE" not in env
    assert env["CCACHE_IGNOREOPTIONS"] == (
        f"-fmacro-prefix-map={source_dir.as_posix()}=CMAKE_SOURCE_DIR"
    )


@pytest.mark.usefixtures("clean_ccache_env")
def test_ccache_env_skips_the_map_entry_on_whitespace(tmp_path: Path) -> None:
    """The ignore list splits on spaces, so a spaced path stays hashed."""
    CORE.build_path = tmp_path / "with space" / "build"
    with patch(
        "esphome.build_helpers.ccache.resolve_ccache_path",
        return_value="/usr/bin/ccache",
    ):
        env = _ccache_env({}, tmp_path / "with space" / "build" / "zephyr")
    assert "CCACHE_IGNOREOPTIONS" not in env


# ---------------------------------------------------------------------------
# --skip-bootloader
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    ("swap_method", "expected"),
    [
        ("offset", ["--domain", "zephyr"]),
        (
            "direct",
            ["--domain", "zephyr", "--domain", "zephyr_slot1_variant"],
        ),
    ],
)
def test_app_domain_args(swap_method: str, expected: list[str]) -> None:
    """Only the app images build; direct-xip has two."""
    with patch(
        "esphome.components.zephyr.mcuboot.zephyr_swap_method",
        return_value=swap_method,
    ):
        assert _app_domain_args() == expected


def _domains(build_dir: Path, *names: str) -> None:
    build_dir.mkdir(parents=True, exist_ok=True)
    (build_dir / "domains.yaml").write_text(
        "domains:\n" + "".join(f"- name: {n}\n" for n in names)
    )


def test_check_bootloader_built_refuses_a_skipped_mcuboot(tmp_path: Path) -> None:
    _domains(tmp_path, "zephyr", "mcuboot")
    with pytest.raises(EsphomeError, match="--skip-bootloader"):
        check_bootloader_built(tmp_path)


def test_check_bootloader_built_accepts_a_built_mcuboot(tmp_path: Path) -> None:
    _domains(tmp_path, "zephyr", "mcuboot")
    elf = tmp_path / "mcuboot" / "zephyr" / "zephyr.elf"
    elf.parent.mkdir(parents=True)
    elf.write_bytes(b"")
    check_bootloader_built(tmp_path)


def test_check_bootloader_built_without_mcuboot(tmp_path: Path) -> None:
    _domains(tmp_path, "zephyr")
    check_bootloader_built(tmp_path)
    check_bootloader_built(tmp_path / "never-built")
