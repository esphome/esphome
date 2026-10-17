"""Tests for esphome.components.nrf52.framework helpers."""

import errno
import hashlib
import os
from pathlib import Path
import sys
from types import SimpleNamespace
from unittest.mock import ANY, call, patch

import platformdirs
import pytest

from esphome.components.nrf52 import _resolve_toolchain, framework
from esphome.components.nrf52.framework import (
    _PLATFORMIO_PENV_REQUIREMENTS,
    _REQUIREMENTS,
    DEFAULT_WEST_PROJECTS,
    TOOLCHAIN_VERSION,
    _get_penv_site_packages,
    _get_platformio_penv_path,
    _get_toolchain_platform_info,
    _install_toolchain,
    _needs_venv_rebuild,
    check_and_install,
    get_build_env,
    get_sdk_nrf_tools_path,
    include_west_project,
    setup_platformio_python_env,
    wanted_west_projects,
)
from esphome.components.zephyr.const import KEY_SYSBUILD, KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.config_validation import Version
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION, Toolchain
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import get_python_env_executable_path


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
# The filter that keeps only DEFAULT_WEST_PROJECTS
_DEFAULTS_FILTER = "-.*,+cmsis,+hal_nordic,+nrfxlib,+zephyr"


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
        # download_and_extract resolves its internals in framework_helpers,
        # so the download/extract seams are patched there.
        patch(
            "esphome.framework_helpers.download_from_mirrors",
            return_value="https://example.com/tc.tar.xz",
        ) as mock_download,
        patch("esphome.framework_helpers.archive_extract_all") as mock_extract,
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


def _mark_west_initialized(framework: Path) -> None:
    """What a finished ``west init`` leaves behind."""
    (framework / ".west").mkdir()
    (framework / ".west" / "config").touch()


def _touch_penv_python(penv: Path) -> None:
    """Create the interpreter file so the rebuild gate sees a live venv."""
    python = get_python_env_executable_path(penv, "python")
    python.parent.mkdir(parents=True, exist_ok=True)
    python.touch()


def _subcommand(cmd: list[str]) -> str:
    tool = "west" if "west" in cmd else "pip"
    return cmd[cmd.index(tool) + 1]


def _subcommands(run_command_ok) -> list[str]:
    """The west or pip subcommand of each command run, in order."""
    return [_subcommand(c.args[0]) for c in run_command_ok.call_args_list]


def _project_filter(run_command_ok) -> str:
    """The manifest.project-filter value of the last ``west config`` run."""
    for west_call in reversed(run_command_ok.call_args_list):
        cmd = west_call.args[0]
        if "manifest.project-filter" in cmd:
            return cmd[-1]
    raise AssertionError("no west config command ran")


def _mark_installed(dirs: SimpleNamespace) -> None:
    """Every install step finished: venv, zephyr requirements, SDK, toolchain."""
    _mark_venv_ready(dirs.python_env)
    (dirs.python_env / ".zephyr_reqs_ready").touch()
    (dirs.framework / ".ready").touch()
    (dirs.toolchain / ".ready").touch()


def _mark_venv_ready(python_env: Path) -> None:
    """Write the venv sentinel with the current requirements hash and a
    present interpreter so the rebuild gate passes."""
    requirements_hash = hashlib.sha256(_REQUIREMENTS.read_bytes()).hexdigest()
    (python_env / ".ready").write_text(requirements_hash, encoding="utf-8")
    _touch_penv_python(python_env)


class TestCheckAndInstall:
    def test_all_installed_skips_all_steps(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """All three sentinels present → nothing downloaded or compiled."""
        _mark_installed(nrf52_dirs)

        check_and_install()

        mock_nrf52_ops.create_venv.assert_not_called()
        mock_nrf52_ops.run_command_ok.assert_not_called()
        mock_nrf52_ops.download_from_mirrors.assert_not_called()
        mock_nrf52_ops.archive_extract_all.assert_not_called()

    def test_missing_interpreter_rebuilds_venv(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A valid sentinel must not mask a missing interpreter (a cached venv
        restored after a host interpreter upgrade)."""
        requirements_hash = hashlib.sha256(_REQUIREMENTS.read_bytes()).hexdigest()
        (nrf52_dirs.python_env / ".ready").write_text(
            requirements_hash, encoding="utf-8"
        )
        # no interpreter on disk

        check_and_install()

        mock_nrf52_ops.create_venv.assert_called_once()

    def test_fresh_install_runs_all_steps(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """No sentinels → venv created, west installed, SDK init+update, toolchain downloaded."""
        check_and_install()

        mock_nrf52_ops.create_venv.assert_called_once()
        assert _subcommands(mock_nrf52_ops.run_command_ok) == [
            "install",  # requirements
            "init",
            "config",
            "update",
            "list",
            "install",  # zephyr requirements
        ]
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
        assert _subcommands(mock_nrf52_ops.run_command_ok) == [
            "init",
            "config",
            "update",
            "list",
            "install",
        ]
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

    def test_framework_clone_is_shallow(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Both the manifest repository and every project are fetched at depth 1."""
        _mark_venv_ready(nrf52_dirs.python_env)

        check_and_install()

        init, _, update = mock_nrf52_ops.run_command_ok.call_args_list[:3]
        assert "-o=--depth=1" in init.args[0]
        assert "--fetch-opt=--depth=1" in update.args[0]
        # Streamed, so the long clone's progress reaches the log
        assert init.kwargs["stream_output"] is True
        assert update.kwargs["stream_output"] is True

    def test_interrupted_download_resumes(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A workspace left by a cut-short download is updated in place, not
        wiped and cloned again."""
        _mark_venv_ready(nrf52_dirs.python_env)
        _mark_west_initialized(nrf52_dirs.framework)

        # A marker left by an earlier failed resume
        (nrf52_dirs.framework / ".resume_failed").touch()

        check_and_install()

        assert (
            call(nrf52_dirs.framework, msg=ANY)
            not in mock_nrf52_ops.rmdir.call_args_list
        )
        assert not (nrf52_dirs.framework / ".resume_failed").exists()
        # west update in the workspace (no init), then pip install zephyr reqs
        assert _subcommands(mock_nrf52_ops.run_command_ok) == [
            "config",
            "update",
            "list",
            "install",
        ]
        update = mock_nrf52_ops.run_command_ok.call_args_list[1]
        assert update.kwargs["cwd"] == nrf52_dirs.framework
        assert (nrf52_dirs.framework / ".ready").exists()

    def test_failed_resume_keeps_the_download_once(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A first failed resume keeps what was fetched (the network likely
        dropped again) and is retried on the next build."""
        _mark_venv_ready(nrf52_dirs.python_env)
        _mark_west_initialized(nrf52_dirs.framework)
        # config succeeds, the resumed update fails
        mock_nrf52_ops.run_command_ok.side_effect = [True, False]

        with pytest.raises(EsphomeError, match="Can't resume"):
            check_and_install()

        assert (
            call(nrf52_dirs.framework, msg=ANY)
            not in mock_nrf52_ops.rmdir.call_args_list
        )
        assert (nrf52_dirs.framework / ".resume_failed").exists()

    def test_cut_short_init_starts_over(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A ``.west`` without its config (init cut short) clones clean."""
        _mark_venv_ready(nrf52_dirs.python_env)
        (nrf52_dirs.framework / ".west").mkdir()

        check_and_install()

        mock_nrf52_ops.rmdir.assert_any_call(nrf52_dirs.framework, msg=ANY)
        first = mock_nrf52_ops.run_command_ok.call_args_list[0]
        assert "init" in first.args[0]

    def test_second_failed_resume_starts_over(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A resume failing twice in a row wipes the workspace and clones clean."""
        _mark_venv_ready(nrf52_dirs.python_env)
        _mark_west_initialized(nrf52_dirs.framework)
        (nrf52_dirs.framework / ".resume_failed").touch()
        # resumed update fails; the clean clone and zephyr reqs succeed
        mock_nrf52_ops.run_command_ok.side_effect = [True, False, *[True] * 5]

        check_and_install()

        mock_nrf52_ops.rmdir.assert_any_call(nrf52_dirs.framework, msg=ANY)
        assert _subcommands(mock_nrf52_ops.run_command_ok) == [
            "config",
            "update",
            "init",
            "config",
            "update",
            "list",
            "install",
        ]

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
        # init and config succeed, update fails
        mock_nrf52_ops.run_command_ok.side_effect = [True, True, False]

        with pytest.raises(EsphomeError, match="Can't update"):
            check_and_install()

    def test_fresh_install_fetches_only_default_projects(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A fresh clone leaves every west project out except the defaults."""
        _mark_venv_ready(nrf52_dirs.python_env)

        check_and_install()

        assert _project_filter(mock_nrf52_ops.run_command_ok) == (
            "-.*,+cmsis,+hal_nordic,+nrfxlib,+zephyr"
        )
        stamp = nrf52_dirs.framework / ".west_projects"
        assert stamp.read_text(encoding="utf-8").split() == sorted(
            DEFAULT_WEST_PROJECTS
        )

    def test_included_project_joins_the_filter(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A project a component includes is fetched with the defaults."""
        _mark_venv_ready(nrf52_dirs.python_env)
        include_west_project("oberon-psa-crypto")

        check_and_install()

        assert "+oberon-psa-crypto" in _project_filter(
            mock_nrf52_ops.run_command_ok
        ).split(",")

    def test_installed_sdk_fetches_a_newly_needed_project(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A finished install gains a project another config left out, keeping
        what it already has."""
        _mark_installed(nrf52_dirs)
        (nrf52_dirs.framework / ".west_projects").write_text(
            "cmsis\nhal_nordic\nnrfxlib\ntinycrypt\nzephyr", encoding="utf-8"
        )
        include_west_project("openthread")

        check_and_install()

        # The names are checked before anything is fetched
        assert _subcommands(mock_nrf52_ops.run_command_ok) == [
            "list",
            "config",
            "update",
        ]
        wanted = "-.*,+cmsis,+hal_nordic,+nrfxlib,+openthread,+tinycrypt,+zephyr"
        assert _project_filter(mock_nrf52_ops.run_command_ok) == wanted
        mock_nrf52_ops.rmdir.assert_not_called()

    @pytest.mark.parametrize(
        "stamp",
        [
            pytest.param(None, id="install_from_before_the_filter"),
            pytest.param(
                "cmsis\nhal_nordic\nnrfxlib\nopenthread\nzephyr",
                id="project_already_fetched",
            ),
        ],
    )
    def test_installed_sdk_with_the_project_fetches_nothing(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
        stamp: str | None,
    ) -> None:
        """No fetch when the install already has every wanted project; an
        install without the stamp (west config, no filter) has them all. The
        names are still checked, which only reads the manifest."""
        _mark_installed(nrf52_dirs)
        _mark_west_initialized(nrf52_dirs.framework)
        if stamp is not None:
            (nrf52_dirs.framework / ".west_projects").write_text(
                stamp, encoding="utf-8"
            )
        include_west_project("openthread")

        check_and_install()

        assert _subcommands(mock_nrf52_ops.run_command_ok) == ["list"]

    def test_sysbuild_fetches_mcuboot(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Sysbuild always builds the MCUboot image, so it needs the project."""
        _mark_venv_ready(nrf52_dirs.python_env)
        CORE.data[KEY_ZEPHYR] = {KEY_SYSBUILD: True}

        check_and_install()

        assert "+mcuboot" in _project_filter(mock_nrf52_ops.run_command_ok).split(",")

    @pytest.mark.parametrize(
        ("sdk_version", "has_cmsis_6"),
        [("2.9.2", False), ("3.1.0", True), ("3.2.0", True)],
    )
    def test_sdk_3_1_and_later_want_cmsis_6(
        self, setup_core: Path, sdk_version: str, has_cmsis_6: bool
    ) -> None:
        """Zephyr 4.1 moved the Cortex-M core headers to the cmsis_6 module."""
        CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: Version.parse(sdk_version)}

        assert ("cmsis_6" in wanted_west_projects()) is has_cmsis_6

    def test_default_projects_never_read_the_stamp(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A build wanting only the defaults has them on any install; an
        unreadable stamp shows the check never looked."""
        _mark_installed(nrf52_dirs)
        (nrf52_dirs.framework / ".west_projects").mkdir()

        check_and_install()

        mock_nrf52_ops.run_command_ok.assert_not_called()

    def test_failed_project_fetch_raises(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A failed fetch of a newly needed project raises, keeps the stamp and
        puts the workspace filter back to the stamp's projects and the defaults."""
        _mark_venv_ready(nrf52_dirs.python_env)
        (nrf52_dirs.framework / ".ready").touch()
        stamp = nrf52_dirs.framework / ".west_projects"
        stamp.write_text("zephyr", encoding="utf-8")
        include_west_project("openthread")
        # list and config succeed, update fails, the restoring config succeeds
        mock_nrf52_ops.run_command_ok.side_effect = [True, True, False, True]

        with pytest.raises(EsphomeError, match="Can't update"):
            check_and_install()

        assert stamp.read_text(encoding="utf-8") == "zephyr"
        assert _subcommands(mock_nrf52_ops.run_command_ok) == [
            "list",
            "config",
            "update",
            "config",
        ]
        # The defaults always stay in the restored filter
        assert _project_filter(mock_nrf52_ops.run_command_ok) == _DEFAULTS_FILTER

    def test_failed_fetch_with_a_lost_stamp_keeps_the_defaults_active(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """An unknown installed set must not leave a filter with every module off."""
        _mark_installed(nrf52_dirs)
        _mark_west_initialized(nrf52_dirs.framework)
        (nrf52_dirs.framework / ".west" / "config").write_text(
            "[manifest]\nproject-filter = -.*,+zephyr\n", encoding="utf-8"
        )
        include_west_project("openthread")
        # list and config succeed, update fails, the restoring config succeeds
        mock_nrf52_ops.run_command_ok.side_effect = [True, True, False, True]

        with pytest.raises(EsphomeError, match="Can't update"):
            check_and_install()

        assert _project_filter(mock_nrf52_ops.run_command_ok) == _DEFAULTS_FILTER

    def test_failed_filter_restore_is_logged(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
        caplog: pytest.LogCaptureFixture,
    ) -> None:
        """When the filter can't be put back after a failed fetch, the user is told."""
        _mark_venv_ready(nrf52_dirs.python_env)
        (nrf52_dirs.framework / ".ready").touch()
        (nrf52_dirs.framework / ".west_projects").write_text("zephyr", encoding="utf-8")
        include_west_project("openthread")
        # list and config succeed, update fails, the restoring config fails too
        mock_nrf52_ops.run_command_ok.side_effect = [True, True, False, False]

        with pytest.raises(EsphomeError, match="Can't update"):
            check_and_install()

        assert "Couldn't put the nRF Connect SDK" in caplog.text

    def test_lost_stamp_on_a_filtered_install_fetches_again(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A workspace with a project filter but no stamp is a filtered install
        whose record was lost, so the wanted projects are fetched, not assumed."""
        _mark_installed(nrf52_dirs)
        _mark_west_initialized(nrf52_dirs.framework)
        (nrf52_dirs.framework / ".west" / "config").write_text(
            "[manifest]\nproject-filter = -.*,+zephyr\n", encoding="utf-8"
        )
        include_west_project("openthread")

        check_and_install()

        assert _subcommands(mock_nrf52_ops.run_command_ok) == [
            "list",
            "config",
            "update",
        ]

    def test_install_waits_for_another_build_holding_the_lock(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
        caplog: pytest.LogCaptureFixture,
    ) -> None:
        """A held lock is waited on, never mistaken for a filesystem that
        cannot lock (filelock's Timeout is an OSError as well)."""
        from filelock import Timeout

        _mark_installed(nrf52_dirs)

        with (
            caplog.at_level("INFO"),
            patch("filelock.FileLock") as file_lock,
        ):
            file_lock.return_value.acquire.side_effect = [Timeout("install.lock"), None]
            check_and_install()

        assert file_lock.return_value.acquire.call_count == 2
        assert "Waiting for another build" in caplog.text
        assert "continuing without a lock" not in caplog.text

    def test_install_from_before_the_filter_still_checks_the_names(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """An install with every project fetches nothing, but an unknown name
        is still rejected so a mistake shows on every install alike."""
        _mark_installed(nrf52_dirs)
        _mark_west_initialized(nrf52_dirs.framework)
        include_west_project("openthread")

        check_and_install()

        assert _subcommands(mock_nrf52_ops.run_command_ok) == ["list"]

        mock_nrf52_ops.run_command_ok.reset_mock()
        mock_nrf52_ops.run_command_ok.return_value = False
        include_west_project("no_such_project")
        with pytest.raises(EsphomeError, match="west list failed"):
            check_and_install()

    def test_install_lock_is_per_sdk_version(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Installs of different SDK versions do not wait on each other; the
        toolchain they share is only locked while it is missing."""
        _mark_installed(nrf52_dirs)

        with patch("filelock.FileLock") as file_lock:
            check_and_install()

        lock_files = [Path(c.args[0]).name for c in file_lock.call_args_list]
        assert lock_files == [f"sdk-v{_TEST_SDK_VERSION}.lock"]

    def test_install_runs_unlocked_where_the_filesystem_cannot_lock(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
        caplog: pytest.LogCaptureFixture,
    ) -> None:
        """No soft-lock fallback: its marker outlives a killed build and would
        hang every later one, so the install goes ahead without a lock."""
        _mark_installed(nrf52_dirs)

        with patch("filelock.FileLock") as file_lock:
            file_lock.return_value.acquire.side_effect = OSError(
                errno.ENOSYS, "Function not implemented"
            )
            check_and_install()

        assert file_lock.call_args.kwargs == {"fallback_to_soft": False}
        assert "continuing without a lock" in caplog.text

    def test_unknown_project_raises_before_anything_is_written(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A filter naming a project the manifest lacks fetches nothing, so
        the names are checked once the update resolved the manifest, and an
        unknown one is never recorded as installed."""
        _mark_venv_ready(nrf52_dirs.python_env)
        include_west_project("no_such_project")
        # init, config and update succeed, list fails
        mock_nrf52_ops.run_command_ok.side_effect = [True, True, True, False]

        with pytest.raises(EsphomeError, match="west list failed .*no_such_project"):
            check_and_install()

        assert _subcommands(mock_nrf52_ops.run_command_ok) == [
            "init",
            "config",
            "update",
            "list",
        ]
        assert "no_such_project" in mock_nrf52_ops.run_command_ok.call_args.args[0]
        assert not (nrf52_dirs.framework / ".west_projects").exists()

    def test_unknown_project_on_an_installed_sdk_fetches_nothing(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """On a finished install the names are checked first, so an unknown one
        costs no fetch and leaves the workspace as it was."""
        _mark_venv_ready(nrf52_dirs.python_env)
        (nrf52_dirs.framework / ".ready").touch()
        (nrf52_dirs.framework / ".west_projects").write_text("zephyr", encoding="utf-8")
        include_west_project("no_such_project")
        # list fails before anything is fetched or changed
        mock_nrf52_ops.run_command_ok.side_effect = [False]

        with pytest.raises(EsphomeError, match="west list failed"):
            check_and_install()

        assert _subcommands(mock_nrf52_ops.run_command_ok) == ["list"]
        assert (nrf52_dirs.framework / ".west_projects").read_text(
            encoding="utf-8"
        ) == "zephyr"

    def test_missing_stamp_and_west_config_fetches_again(
        self,
        nrf52_dirs: SimpleNamespace,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """With neither the stamp nor .west/config left, nothing says what the
        install has, so the wanted projects are fetched rather than assumed."""
        _mark_installed(nrf52_dirs)
        include_west_project("openthread")

        check_and_install()

        assert _subcommands(mock_nrf52_ops.run_command_ok) == [
            "list",
            "config",
            "update",
        ]

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

    def test_toolchain_download_uses_gnu_url_for_sdk_3_4_0(
        self,
        tmp_path: Path,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """For nRF Connect SDK >= 3.4.0 the toolchain archive name includes 'toolchain_gnu_'."""
        CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: Version.parse("3.4.0")}
        sdk_version = "3.4.0"
        tools = get_sdk_nrf_tools_path()
        python_env = tools / "penvs" / f"v{sdk_version}"
        framework = tools / "frameworks" / f"v{sdk_version}"
        toolchain_dir = tools / "toolchains" / "1.0.1"
        for d in (python_env, framework, toolchain_dir):
            d.mkdir(parents=True, exist_ok=True)
        (framework / "zephyr" / "scripts").mkdir(parents=True, exist_ok=True)
        (framework / "zephyr" / "scripts" / "requirements.txt").touch()
        _mark_venv_ready(python_env)
        (framework / ".ready").touch()

        check_and_install()

        # Two download calls: minimal SDK first, toolchain second
        toolchain_call = mock_nrf52_ops.download_from_mirrors.call_args_list[1]
        mirrors = toolchain_call.args[0]
        assert all("toolchain_gnu_" in m for m in mirrors)

    def test_toolchain_extracts_under_gnu_for_sdk_3_4_0(
        self,
        tmp_path: Path,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """SDK 1.0+ toolchain archive must land in gnu/arm-zephyr-eabi/.

        Zephyr-sdkConfig.cmake validates the toolchain at gnu/arm-zephyr-eabi/
        in SDK 1.0+; if the archive is extracted to arm-zephyr-eabi/ instead,
        cmake reports the package as not found.
        """
        CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: Version.parse("3.4.0")}
        sdk_version = "3.4.0"
        tools = get_sdk_nrf_tools_path()
        python_env = tools / "penvs" / f"v{sdk_version}"
        framework = tools / "frameworks" / f"v{sdk_version}"
        toolchain_dir = tools / "toolchains" / "1.0.1"
        for d in (python_env, framework, toolchain_dir):
            d.mkdir(parents=True, exist_ok=True)
        (framework / "zephyr" / "scripts").mkdir(parents=True, exist_ok=True)
        (framework / "zephyr" / "scripts" / "requirements.txt").touch()
        _mark_venv_ready(python_env)
        (framework / ".ready").touch()

        check_and_install()

        # Two extract calls: minimal SDK first (to toolchain root), toolchain second
        extract_calls = mock_nrf52_ops.archive_extract_all.call_args_list
        _, toolchain_extract_dir = extract_calls[1].args[:2]
        assert toolchain_extract_dir == toolchain_dir / "gnu" / "arm-zephyr-eabi"


# ---------------------------------------------------------------------------
# setup_platformio_python_env tests
# ---------------------------------------------------------------------------


def _platformio_requirements_hash() -> str:
    return hashlib.sha256(
        _REQUIREMENTS.read_bytes()
        + "\n".join(_PLATFORMIO_PENV_REQUIREMENTS).encode()
        + f"python{sys.version_info.major}.{sys.version_info.minor}".encode()
    ).hexdigest()


@pytest.fixture
def platformio_penv_dir() -> Path:
    """Pre-create the PlatformIO penv dir so sentinel writes succeed.

    create_venv is mocked in these tests, so the directory it would have
    created must exist for ``sentinel.write_text`` to work.
    """
    penv_path = _get_platformio_penv_path()
    penv_path.mkdir(parents=True, exist_ok=True)
    return penv_path


class TestSetupPlatformioPythonEnv:
    def test_fresh_install_creates_venv_and_sets_env(
        self,
        platformio_penv_dir: Path,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """No sentinel → venv created, requirements installed, env exported."""
        with patch.dict(os.environ):
            os.environ.pop("PYTHONPATH", None)

            setup_platformio_python_env()

            mock_nrf52_ops.rmdir.assert_called_once()
            mock_nrf52_ops.create_venv.assert_called_once_with(
                platformio_penv_dir, msg="PlatformIO toolchain"
            )
            mock_nrf52_ops.run_command_ok.assert_called_once()
            cmd = mock_nrf52_ops.run_command_ok.call_args[0][0]
            assert cmd[1:4] == ["-m", "pip", "install"]
            assert "-r" in cmd
            assert str(_REQUIREMENTS) in cmd
            for requirement in _PLATFORMIO_PENV_REQUIREMENTS:
                assert requirement in cmd
            sentinel = platformio_penv_dir / ".ready"
            assert sentinel.read_text(encoding="utf-8") == (
                _platformio_requirements_hash()
            )

            assert os.environ["VIRTUAL_ENV"] == str(platformio_penv_dir)
            site_packages = str(_get_penv_site_packages(platformio_penv_dir))
            assert os.environ["PYTHONPATH"] == site_packages
            bin_dir = str(
                get_python_env_executable_path(platformio_penv_dir, "python").parent
            )
            assert os.environ["PATH"].split(os.pathsep)[0] == bin_dir

    def test_ready_sentinel_skips_install_but_sets_env(
        self,
        platformio_penv_dir: Path,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Current sentinel → no install work, env vars still exported."""
        (platformio_penv_dir / ".ready").write_text(
            _platformio_requirements_hash(), encoding="utf-8"
        )
        _touch_penv_python(platformio_penv_dir)

        with patch.dict(os.environ):
            setup_platformio_python_env()

            mock_nrf52_ops.rmdir.assert_not_called()
            mock_nrf52_ops.create_venv.assert_not_called()
            mock_nrf52_ops.run_command_ok.assert_not_called()
            assert os.environ["VIRTUAL_ENV"] == str(platformio_penv_dir)

    def test_stale_sentinel_reinstalls(
        self,
        platformio_penv_dir: Path,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A sentinel from different requirements → venv rebuilt from scratch."""
        sentinel = platformio_penv_dir / ".ready"
        sentinel.write_text("stale-hash", encoding="utf-8")

        with patch.dict(os.environ):
            setup_platformio_python_env()

        mock_nrf52_ops.rmdir.assert_called_once()
        mock_nrf52_ops.create_venv.assert_called_once()
        mock_nrf52_ops.run_command_ok.assert_called_once()
        assert sentinel.read_text(encoding="utf-8") == _platformio_requirements_hash()

    def test_install_failure_raises(
        self,
        platformio_penv_dir: Path,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Failing pip install raises EsphomeError and writes no sentinel."""
        mock_nrf52_ops.run_command_ok.return_value = False

        with (
            patch.dict(os.environ),
            pytest.raises(
                EsphomeError, match="Install requirements for PlatformIO toolchain"
            ),
        ):
            setup_platformio_python_env()

        assert not (platformio_penv_dir / ".ready").exists()

    def test_missing_interpreter_reinstalls(
        self,
        platformio_penv_dir: Path,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A valid sentinel must not mask a missing interpreter."""
        (platformio_penv_dir / ".ready").write_text(
            _platformio_requirements_hash(), encoding="utf-8"
        )
        # no interpreter on disk

        with patch.dict(os.environ):
            setup_platformio_python_env()

        mock_nrf52_ops.create_venv.assert_called_once()

    def test_repeated_calls_do_not_duplicate_env_entries(
        self,
        platformio_penv_dir: Path,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """Compile then upload in one process must not grow PYTHONPATH/PATH."""
        (platformio_penv_dir / ".ready").write_text(
            _platformio_requirements_hash(), encoding="utf-8"
        )
        _touch_penv_python(platformio_penv_dir)
        site_packages = str(_get_penv_site_packages(platformio_penv_dir))
        bin_dir = str(
            get_python_env_executable_path(platformio_penv_dir, "python").parent
        )

        with patch.dict(os.environ):
            setup_platformio_python_env()
            setup_platformio_python_env()

            assert os.environ["PYTHONPATH"].split(os.pathsep).count(site_packages) == 1
            assert os.environ["PATH"].split(os.pathsep).count(bin_dir) == 1

    def test_existing_pythonpath_preserved(
        self,
        platformio_penv_dir: Path,
        mock_nrf52_ops: SimpleNamespace,
    ) -> None:
        """A pre-existing PYTHONPATH keeps its entries after the venv entry."""
        (platformio_penv_dir / ".ready").write_text(
            _platformio_requirements_hash(), encoding="utf-8"
        )
        _touch_penv_python(platformio_penv_dir)
        site_packages = str(_get_penv_site_packages(platformio_penv_dir))

        with patch.dict(os.environ, {"PYTHONPATH": "/existing/path"}):
            setup_platformio_python_env()

            assert os.environ["PYTHONPATH"] == os.pathsep.join(
                [site_packages, "/existing/path"]
            )


@pytest.mark.parametrize(
    ("os_name", "expected_parts"),
    [
        (
            "posix",
            (
                "lib",
                f"python{sys.version_info.major}.{sys.version_info.minor}",
                "site-packages",
            ),
        ),
        ("nt", ("Lib", "site-packages")),
    ],
)
def test_get_penv_site_packages(
    tmp_path: Path, os_name: str, expected_parts: tuple[str, ...]
) -> None:
    penv_path = tmp_path / "penv"
    with patch("os.name", os_name):
        assert _get_penv_site_packages(penv_path) == penv_path.joinpath(*expected_parts)


# ---------------------------------------------------------------------------
# get_build_env tests
# ---------------------------------------------------------------------------


def test_get_build_env(
    nrf52_dirs: SimpleNamespace, monkeypatch: pytest.MonkeyPatch
) -> None:
    """get_build_env exposes ZEPHYR_SDK_INSTALL_DIR pointing at the toolchain root.

    ZEPHYR_SDK_INSTALL_DIR is the variable Zephyr's FindZephyr-sdk.cmake
    explicitly consumes (from the environment) and uses as a find_package
    HINT. The old Zephyr-sdk_DIR environment hint proved unreliable in
    containerized non-root builds and was removed.
    """
    monkeypatch.setenv("SOME_PREEXISTING_VAR", "kept")
    monkeypatch.delenv("CCACHE_DISABLE", raising=False)

    env = get_build_env(None)

    tools = get_sdk_nrf_tools_path()
    venv_bin_dir = get_python_env_executable_path(
        tools / "penvs" / f"v{_TEST_SDK_VERSION}", "python"
    ).parent
    assert env["PATH"].startswith(str(venv_bin_dir) + os.pathsep)
    assert env["ZEPHYR_BASE"] == str(
        tools / "frameworks" / f"v{_TEST_SDK_VERSION}" / "zephyr"
    )
    # Toolchain root, not the cmake/ subdir
    assert env["ZEPHYR_SDK_INSTALL_DIR"] == str(
        tools / "toolchains" / TOOLCHAIN_VERSION
    )
    assert "Zephyr-sdk_DIR" not in env
    # The rest of the process environment is inherited
    assert env["SOME_PREEXISTING_VAR"] == "kept"
    # No managed settings without a resolved binary; the self-enabled
    # Zephyr ccache must not cache
    assert "CCACHE_DIR" not in env or "CCACHE_DIR" in os.environ
    assert env["CCACHE_DISABLE"] == "1"


def test_get_build_env_with_ccache(
    nrf52_dirs: SimpleNamespace, monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """A resolved ccache brings the shared managed settings."""
    for key in (
        "CCACHE_DIR",
        "CCACHE_DEPEND",
        "CCACHE_NOHASHDIR",
        "CCACHE_BASEDIR",
        "CCACHE_DISABLE",
    ):
        monkeypatch.delenv(key, raising=False)
    CORE.build_path = tmp_path / "build"
    env = get_build_env("/usr/bin/ccache")
    assert env["CCACHE_DIR"] == str(get_sdk_nrf_tools_path() / "ccache")
    assert env["CCACHE_DEPEND"] == "1"
    assert env["CCACHE_BASEDIR"] == str((tmp_path / "build").resolve())
    assert "CCACHE_DISABLE" not in env
    # Only the per build map entry leaves the hash; user maps stay in
    assert env["CCACHE_IGNOREOPTIONS"] == (
        f"-fmacro-prefix-map={(tmp_path / 'build' / 'zephyr').as_posix()}"
        "=CMAKE_SOURCE_DIR"
    )


def test_get_build_env_skips_the_map_entry_on_whitespace(
    nrf52_dirs: SimpleNamespace, monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """The ignore list splits on spaces; a spaced path cannot be
    expressed, so the entry is left hashed rather than emitted broken."""
    monkeypatch.delenv("CCACHE_IGNOREOPTIONS", raising=False)
    CORE.build_path = tmp_path / "with space" / "build"
    env = get_build_env("/usr/bin/ccache")
    assert "CCACHE_IGNOREOPTIONS" not in env


def test_get_build_env_sdk_3_4_0_uses_toolchain_root(
    setup_core: Path,
) -> None:
    """For NCS >= 3.4.0, ZEPHYR_SDK_INSTALL_DIR still points at the toolchain root."""
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: Version.parse("3.4.0")}

    env = get_build_env(None)

    tools = get_sdk_nrf_tools_path()
    assert env["ZEPHYR_SDK_INSTALL_DIR"] == str(tools / "toolchains" / "1.0.1")
    assert "Zephyr-sdk_DIR" not in env


def test_install_toolchain_keeps_other_live_toolchain_leftovers(
    setup_core: Path,
) -> None:
    """Pruning must not delete a partial download of the other toolchain in use."""
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: Version.parse("3.4.0")}
    toolchains = get_sdk_nrf_tools_path() / "toolchains"
    toolchains.mkdir(parents=True)
    own = toolchains / "1.0.1.toolchain.archive.part"
    other = toolchains / f"{TOOLCHAIN_VERSION}.toolchain.archive.part"
    retired = toolchains / "0.16.8.toolchain.archive.part"
    for leftover in (own, other, retired):
        leftover.write_text("")

    with patch(
        "esphome.components.nrf52.framework.download_and_extract",
        side_effect=lambda *args, **kwargs: args[3].mkdir(parents=True),
    ):
        _install_toolchain()

    assert not own.exists()
    assert other.exists()
    assert not retired.exists()
    assert (toolchains / "1.0.1" / ".ready").exists()


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

    monkeypatch.setenv("ESPHOME_SDK_NRF_PREFIX", value)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "sdk-nrf"
    ).resolve()
    assert get_sdk_nrf_tools_path() == expected


def testget_tools_path_default_is_global_cache(
    monkeypatch: pytest.MonkeyPatch,
) -> None:

    monkeypatch.delenv("ESPHOME_SDK_NRF_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "sdk-nrf"
    ).resolve()
    assert get_sdk_nrf_tools_path() == expected


def test_needs_venv_rebuild_gates(tmp_path: Path) -> None:
    """The shared penv gate rebuilds on any missing or stale piece."""
    penv = tmp_path / "penv"
    penv.mkdir()
    python = penv / "python"
    sentinel = penv / ".ready"
    good_hash = "abc123"

    # Nothing in place yet
    assert _needs_venv_rebuild(python, sentinel, good_hash)

    python.write_text("")
    # Interpreter present but no sentinel
    assert _needs_venv_rebuild(python, sentinel, good_hash)

    sentinel.write_text(good_hash, encoding="utf-8")
    # Everything in place
    assert not _needs_venv_rebuild(python, sentinel, good_hash)

    # Stale requirements hash
    assert _needs_venv_rebuild(python, sentinel, "otherhash")


@pytest.mark.skipif(
    sys.platform == "win32", reason="symlink creation needs privileges on Windows"
)
def test_needs_venv_rebuild_on_dangling_interpreter_symlink(tmp_path: Path) -> None:
    """A cached venv restored after a host interpreter upgrade has a
    bin/python symlink whose target is gone; the valid sentinel must not
    mask it."""
    penv = tmp_path / "penv"
    penv.mkdir()
    python = penv / "python"
    sentinel = penv / ".ready"
    sentinel.write_text("abc123", encoding="utf-8")
    python.symlink_to(tmp_path / "hostedtoolcache" / "3.12.14" / "python3")
    assert python.is_symlink()
    assert not python.exists()

    assert _needs_venv_rebuild(python, sentinel, "abc123")


def test_resolve_toolchain_rejects_unsupported() -> None:
    """A --toolchain nRF52 cannot serve fails instead of degrading silently."""

    CORE.toolchain = Toolchain.ARDUINO
    with pytest.raises(cv.Invalid, match="Unsupported toolchain 'arduino'"):
        _resolve_toolchain({})


def test_patch_gen_defines_relativizes_the_dts_path(tmp_path: Path) -> None:
    """The absolute dts.pre path is the only per device byte in the
    devicetree header; the patch makes gen_defines emit the basename."""
    gen = tmp_path / "zephyr" / "scripts" / "dts" / "gen_defines.py"
    gen.parent.mkdir(parents=True)
    gen.write_text("s = f'DTS input file:\\n  {edt.dts_path}\\n'\n")
    framework._patch_gen_defines_dts_path(tmp_path)
    assert "{os.path.basename(edt.dts_path)}" in gen.read_text()
    assert not list(gen.parent.glob("*.tmp"))  # no leftovers
    before = gen.read_text()
    framework._patch_gen_defines_dts_path(tmp_path)  # idempotent
    assert gen.read_text() == before
    framework._patch_gen_defines_dts_path(tmp_path / "absent")  # tolerant


def test_patch_gen_defines_warns_when_the_anchor_is_gone(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A reformatted upstream must not silently cost the sharing."""
    gen = tmp_path / "zephyr" / "scripts" / "dts" / "gen_defines.py"
    gen.parent.mkdir(parents=True)
    gen.write_text("s = 'something else entirely'\n")
    with caplog.at_level("WARNING"):
        framework._patch_gen_defines_dts_path(tmp_path)
    assert "gen_defines.py no longer matches" in caplog.text
