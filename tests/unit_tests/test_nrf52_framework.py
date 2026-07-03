"""Tests for esphome.components.nrf52.framework helpers."""

import hashlib
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from esphome.components.nrf52.framework import (
    _REQUIREMENTS,
    TOOLCHAIN_VERSION,
    _get_toolchain_platform_info,
    check_and_install,
    get_sdk_nrf_tools_path,
)
from esphome.config_validation import Version
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, EsphomeError


@pytest.fixture(autouse=True)
def _isolate_sdk_nrf_install_path(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Pin the sdk-nrf install root to a tmp dir for every test.

    The default location is the OS user cache dir, so without this any test
    that builds framework paths or pre-creates the install dir would touch
    the real ``~/.cache/esphome`` on the developer's machine. Tests that need
    to exercise the override or default-resolution logic clear/override the
    env themselves.
    """
    monkeypatch.setenv("ESPHOME_SDK_NRF_PREFIX", str(tmp_path / "sdk_nrf_install"))


@pytest.mark.parametrize(
    ("system", "machine", "expected"),
    [
        # default — no branch hit
        ("Linux", "x86_64", ("linux", "x86_64", "tar.xz")),
        # arm64 → aarch64 rename
        ("Linux", "arm64", ("linux", "aarch64", "tar.xz")),
        # darwin → macos rename only
        ("Darwin", "x86_64", ("macos", "x86_64", "tar.xz")),
        # both renames apply
        ("Darwin", "arm64", ("macos", "aarch64", "tar.xz")),
        # windows forces x86_64 + 7z; arm64 rename is overwritten
        ("Windows", "arm64", ("windows", "x86_64", "7z")),
    ],
)
def test_get_toolchain_platform_info(
    system: str, machine: str, expected: tuple[str, str, str]
) -> None:
    with (
        patch("platform.system", return_value=system),
        patch("platform.machine", return_value=machine),
    ):
        assert _get_toolchain_platform_info() == expected


# ---------------------------------------------------------------------------
# Helpers and fixtures for check_and_install tests
# ---------------------------------------------------------------------------

_TEST_SDK_VERSION = "2.9.0"


@pytest.fixture
def nrf52_dirs(setup_core: Path) -> SimpleNamespace:
    """Populate CORE and pre-create SDK directories so sentinel.touch() succeeds."""
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: Version.parse(_TEST_SDK_VERSION)}
    tools = get_sdk_nrf_tools_path()
    python_env = tools / "penvs" / f"v{_TEST_SDK_VERSION}"
    framework = tools / "frameworks" / f"v{_TEST_SDK_VERSION}"
    toolchain_dir = tools / "toolchains" / TOOLCHAIN_VERSION
    for d in (python_env, framework, toolchain_dir):
        d.mkdir(parents=True, exist_ok=True)
    zephyr_scripts = framework / "zephyr" / "scripts"
    zephyr_scripts.mkdir(parents=True, exist_ok=True)
    (zephyr_scripts / "requirements.txt").touch()
    return SimpleNamespace(
        python_env=python_env,
        framework=framework,
        toolchain=toolchain_dir,
    )


@pytest.fixture
def mock_nrf52_ops():
    """Patch all heavy I/O operations used by check_and_install."""
    with (
        patch("esphome.components.nrf52.framework.rmdir") as mock_rmdir,
        patch("esphome.components.nrf52.framework.create_venv") as mock_create_venv,
        patch(
            "esphome.components.nrf52.framework.run_command_ok", return_value=True
        ) as mock_run_cmd,
        patch(
            "esphome.components.nrf52.framework.download_from_mirrors",
            return_value="https://example.com/tc.tar.xz",
        ) as mock_download,
        patch("esphome.components.nrf52.framework.archive_extract_all") as mock_extract,
    ):
        yield SimpleNamespace(
            rmdir=mock_rmdir,
            create_venv=mock_create_venv,
            run_command_ok=mock_run_cmd,
            download_from_mirrors=mock_download,
            archive_extract_all=mock_extract,
        )


# ---------------------------------------------------------------------------
# check_and_install tests
# ---------------------------------------------------------------------------


def _mark_venv_ready(python_env: Path) -> None:
    """Write the venv sentinel with the current requirements hash."""
    requirements_hash = hashlib.sha256(_REQUIREMENTS.read_bytes()).hexdigest()
    (python_env / ".ready").write_text(requirements_hash, encoding="utf-8")


class TestCheckAndInstall:
    def test_all_installed_skips_all_steps(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """All three sentinels present → nothing downloaded or compiled."""
        _mark_venv_ready(nrf52_dirs.python_env)
        (nrf52_dirs.python_env / ".zephyr_reqs_ready").touch()
        (nrf52_dirs.framework / ".ready").touch()
        (nrf52_dirs.toolchain / ".ready").touch()

        check_and_install()

        mock_nrf52_ops.create_venv.assert_not_called()
        mock_nrf52_ops.run_command_ok.assert_not_called()
        mock_nrf52_ops.download_from_mirrors.assert_not_called()
        mock_nrf52_ops.archive_extract_all.assert_not_called()

    def test_fresh_install_runs_all_steps(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """No sentinels → venv created, west installed, SDK init+update, toolchain downloaded."""
        check_and_install()

        mock_nrf52_ops.create_venv.assert_called_once()
        # pip install requirements, west init, west update, pip install zephyr reqs
        assert mock_nrf52_ops.run_command_ok.call_count == 4
        # minimal SDK + per-arch toolchain
        assert mock_nrf52_ops.download_from_mirrors.call_count == 2
        assert mock_nrf52_ops.archive_extract_all.call_count == 2
        assert (nrf52_dirs.python_env / ".ready").exists()
        assert (nrf52_dirs.python_env / ".zephyr_reqs_ready").exists()
        assert (nrf52_dirs.framework / ".ready").exists()
        assert (nrf52_dirs.toolchain / ".ready").exists()

    def test_venv_exists_installs_framework_and_toolchain(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Venv ready but framework missing → skip venv creation, run SDK init+update."""
        _mark_venv_ready(nrf52_dirs.python_env)

        check_and_install()

        mock_nrf52_ops.create_venv.assert_not_called()
        # west init, west update, pip install zephyr reqs
        assert mock_nrf52_ops.run_command_ok.call_count == 3
        # minimal SDK + per-arch toolchain
        assert mock_nrf52_ops.download_from_mirrors.call_count == 2

    def test_toolchain_only_missing(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Venv and framework ready → only toolchain downloaded and extracted."""
        _mark_venv_ready(nrf52_dirs.python_env)
        (nrf52_dirs.python_env / ".zephyr_reqs_ready").touch()
        (nrf52_dirs.framework / ".ready").touch()

        check_and_install()

        mock_nrf52_ops.create_venv.assert_not_called()
        mock_nrf52_ops.run_command_ok.assert_not_called()
        # minimal SDK + per-arch toolchain
        assert mock_nrf52_ops.download_from_mirrors.call_count == 2
        assert mock_nrf52_ops.archive_extract_all.call_count == 2

    def test_requirements_install_failure_raises(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Failing pip install -r requirements.txt raises EsphomeError."""
        mock_nrf52_ops.run_command_ok.return_value = False

        with pytest.raises(EsphomeError, match="Install requirements"):
            check_and_install()

    def test_framework_init_failure_raises(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Failing west init raises EsphomeError."""
        _mark_venv_ready(nrf52_dirs.python_env)
        mock_nrf52_ops.run_command_ok.return_value = False

        with pytest.raises(EsphomeError, match="Can't initialize"):
            check_and_install()

    def test_framework_update_failure_raises(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Failing west update raises EsphomeError."""
        _mark_venv_ready(nrf52_dirs.python_env)
        # init succeeds, update fails
        mock_nrf52_ops.run_command_ok.side_effect = [True, False]

        with pytest.raises(EsphomeError, match="Can't update"):
            check_and_install()

    def test_toolchain_download_passes_platform_substitutions(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """download_from_mirrors receives VERSION + platform triple from _get_toolchain_platform_info."""
        _mark_venv_ready(nrf52_dirs.python_env)
        (nrf52_dirs.framework / ".ready").touch()

        with patch(
            "esphome.components.nrf52.framework._get_toolchain_platform_info",
            return_value=("linux", "x86_64", "tar.xz"),
        ):
            check_and_install()

        args, _ = mock_nrf52_ops.download_from_mirrors.call_args
        substitutions = args[1]
        assert substitutions["VERSION"] == TOOLCHAIN_VERSION
        assert substitutions["sysname"] == "linux"
        assert substitutions["machine"] == "x86_64"
        assert substitutions["extension"] == "tar.xz"


# ---------------------------------------------------------------------------
# get_sdk_nrf_tools_path tests
# ---------------------------------------------------------------------------


def testget_tools_path_env_override(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    override = tmp_path / "custom" / "sdk-nrf"
    monkeypatch.setenv("ESPHOME_SDK_NRF_PREFIX", str(override))
    assert get_sdk_nrf_tools_path() == override.resolve()


@pytest.mark.parametrize("value", ["", "   "])
def testget_tools_path_blank_env_falls_back_to_default(
    value: str, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A blank ESPHOME_SDK_NRF_PREFIX is treated as unset, not as CWD.

    Path("") would resolve to the working directory, which clean-all could
    then delete by accident.
    """
    import platformdirs

    monkeypatch.setenv("ESPHOME_SDK_NRF_PREFIX", value)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "sdk-nrf"
    ).resolve()
    assert get_sdk_nrf_tools_path() == expected


def testget_tools_path_default_is_global_cache(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    import platformdirs

    monkeypatch.delenv("ESPHOME_SDK_NRF_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "sdk-nrf"
    ).resolve()
    assert get_sdk_nrf_tools_path() == expected
