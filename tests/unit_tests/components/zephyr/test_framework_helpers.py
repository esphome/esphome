"""Tests for esphome.components.zephyr.framework_helpers."""

from collections.abc import Iterator
import os
from pathlib import Path
import sys
from typing import Any
from unittest.mock import ANY, MagicMock, patch

import pytest

from esphome.components.zephyr import framework_helpers
from esphome.components.zephyr.framework_helpers import (
    FrameworkSource,
    ensure_venv,
    ensure_zephyr_requirements,
    fetch_missing_west_projects,
    install_framework,
    install_sitecustomize,
    needs_venv_rebuild,
    patch_gen_defines_dts_path,
    patch_west_build_source_dir_relpath,
)
from esphome.core import EsphomeError
from esphome.framework_helpers import get_python_env_executable_path


def test_needs_venv_rebuild_gates(tmp_path: Path) -> None:
    """The shared penv gate rebuilds on any missing or stale piece."""
    penv = tmp_path / "penv"
    penv.mkdir()
    python = penv / "python"
    sentinel = penv / ".ready"
    good_hash = "abc123"

    # Nothing in place yet
    assert needs_venv_rebuild(python, sentinel, good_hash)

    python.write_text("")
    # Interpreter present but no sentinel
    assert needs_venv_rebuild(python, sentinel, good_hash)

    sentinel.write_text(good_hash, encoding="utf-8")
    # Everything in place
    assert not needs_venv_rebuild(python, sentinel, good_hash)

    # Stale requirements hash
    assert needs_venv_rebuild(python, sentinel, "otherhash")


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

    assert needs_venv_rebuild(python, sentinel, "abc123")


def test_install_sitecustomize_writes_patch_on_windows(tmp_path: Path) -> None:
    with patch.object(framework_helpers.os, "name", "nt"):
        install_sitecustomize(tmp_path)

    written = tmp_path / "Lib" / "site-packages" / "sitecustomize.py"
    assert written.read_text(encoding="utf-8") == framework_helpers._SITECUSTOMIZE


def test_install_sitecustomize_does_nothing_elsewhere(tmp_path: Path) -> None:
    with patch.object(framework_helpers.os, "name", "posix"):
        install_sitecustomize(tmp_path)

    assert not any(tmp_path.iterdir())


def test_patch_gen_defines_relativizes_the_dts_path(tmp_path: Path) -> None:
    """The absolute dts.pre path is the only per device byte in the
    devicetree header; the patch makes gen_defines emit the basename."""
    gen = tmp_path / "zephyr" / "scripts" / "dts" / "gen_defines.py"
    gen.parent.mkdir(parents=True)
    gen.write_text("s = f'DTS input file:\\n  {edt.dts_path}\\n'\n")
    patch_gen_defines_dts_path(tmp_path)
    assert "{os.path.basename(edt.dts_path)}" in gen.read_text()
    assert not list(gen.parent.glob("*.tmp"))  # no leftovers
    before = gen.read_text()
    patch_gen_defines_dts_path(tmp_path)  # idempotent
    assert gen.read_text() == before
    patch_gen_defines_dts_path(tmp_path / "absent")  # tolerant


def test_patch_west_build_tolerates_a_source_dir_on_another_drive(
    tmp_path: Path,
) -> None:
    """West build's relative source path raises across Windows drives; the
    patch applies Zephyr main's fallback to the absolute path."""
    build = tmp_path / "zephyr" / "scripts" / "west_commands" / "build.py"
    build.parent.mkdir(parents=True)
    build.write_text(
        "    def _sanity_check_source_dir(self):\n"
        "        srcrel = os.path.relpath(self.source_dir)\n"
        "        self.check_force(True, srcrel)\n"
    )
    patch_west_build_source_dir_relpath(tmp_path)
    patched = build.read_text()
    assert "relative_to(pathlib.Path.cwd(), walk_up=True)" in patched
    assert (
        "        except ValueError:\n            srcrel = self.source_dir\n" in patched
    )
    assert "os.path.relpath" not in patched
    assert "        self.check_force(True, srcrel)\n" in patched
    assert not list(build.parent.glob("*.tmp"))
    patch_west_build_source_dir_relpath(tmp_path)  # idempotent
    assert build.read_text() == patched
    patch_west_build_source_dir_relpath(tmp_path / "absent")  # tolerant


def test_patch_gen_defines_warns_when_the_anchor_is_gone(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A reformatted upstream must not silently cost the sharing."""
    gen = tmp_path / "zephyr" / "scripts" / "dts" / "gen_defines.py"
    gen.parent.mkdir(parents=True)
    gen.write_text("s = 'something else entirely'\n")
    with caplog.at_level("WARNING"):
        patch_gen_defines_dts_path(tmp_path)
    assert "gen_defines.py no longer matches" in caplog.text


def _fake_create_venv(root: Path, msg: str | None = None) -> None:
    python = get_python_env_executable_path(root, "python")
    python.parent.mkdir(parents=True)
    python.touch()


def test_ensure_venv_builds_once_per_requirements(tmp_path: Path) -> None:
    """Built when missing, skipped while current, rebuilt when requirements change."""
    penv = tmp_path / "penv"
    requirements = tmp_path / "requirements.txt"
    requirements.write_text("west==1.5.0\n")
    with (
        patch.object(
            framework_helpers, "create_venv", side_effect=_fake_create_venv
        ) as create,
        patch.object(framework_helpers, "run_command_ok", return_value=True) as pip,
    ):
        assert ensure_venv(penv, requirements, "v1") is True
        assert ensure_venv(penv, requirements, "v1") is False
        requirements.write_text("west==1.6.0\n")
        assert ensure_venv(penv, requirements, "v1") is True

    assert create.call_count == 2
    assert pip.call_args.args[0][-2:] == ["-r", str(requirements)]


def test_ensure_venv_raises_when_pip_fails(tmp_path: Path) -> None:
    penv = tmp_path / "penv"
    requirements = tmp_path / "requirements.txt"
    requirements.write_text("west==1.5.0\n")
    with (
        patch.object(framework_helpers, "create_venv", side_effect=_fake_create_venv),
        patch.object(framework_helpers, "run_command_ok", return_value=False),
        pytest.raises(EsphomeError, match="Install requirements for v1"),
    ):
        ensure_venv(penv, requirements, "v1")

    assert not (penv / ".ready").exists()


SOURCE: FrameworkSource = FrameworkSource(
    name="Test SDK",
    url="https://example.com/sdk",
    optional=("hal_.*",),
    required=frozenset({"zephyr"}),
)
VERSION: str = "v1.0.0"
ENV_PYTHON: Path = Path("/penv/bin/python")


@pytest.fixture
def workspace(tmp_path: Path) -> Path:
    return tmp_path / "frameworks" / "sdk"


@pytest.fixture
def west(workspace: Path) -> Iterator[MagicMock]:
    """run_command_ok for west and pip; west init leaves an initialized workspace."""

    def _run(cmd: list[str], **kwargs: Any) -> bool:
        if "init" in cmd:
            _mark_initialized(workspace)
        return True

    with (
        patch.object(framework_helpers, "rmdir") as rmdir,
        patch.object(framework_helpers, "run_command_ok", side_effect=_run) as run,
    ):
        run.rmdir = rmdir
        yield run


def _mark_initialized(workspace: Path, config: str = "") -> None:
    (workspace / ".west").mkdir(parents=True, exist_ok=True)
    (workspace / ".west" / "config").write_text(config, encoding="utf-8")


def _mark_finished(workspace: Path, stamp: str | None) -> None:
    """A finished install with a filter; ``stamp`` None leaves the stamp out."""
    _mark_initialized(workspace, "[manifest]\nproject-filter = -hal_.*,+zephyr\n")
    (workspace / ".ready").touch()
    if stamp is not None:
        (workspace / ".west_projects").write_text(stamp, encoding="utf-8")


def _subcommands(run: MagicMock) -> list[str]:
    def _sub(cmd: list[str]) -> str:
        return cmd[cmd.index("west" if "west" in cmd else "pip") + 1]

    return [_sub(c.args[0]) for c in run.call_args_list]


def _west_call(run: MagicMock, subcommand: str, nth: int = -1) -> MagicMock:
    return [c for c in run.call_args_list if subcommand in c.args[0]][nth]


def _project_filter(run: MagicMock) -> str:
    return _west_call(run, "config").args[0][-1]


def _install(workspace: Path, projects: set[str]) -> None:
    install_framework(ENV_PYTHON, workspace, SOURCE, VERSION, projects)


def _fetch(workspace: Path, projects: set[str]) -> None:
    fetch_missing_west_projects(ENV_PYTHON, workspace, SOURCE, VERSION, projects)


def test_install_clones_the_manifest_with_the_filter(
    workspace: Path, west: MagicMock
) -> None:
    _install(workspace, {"zephyr", "hal_a"})

    west.rmdir.assert_called_once_with(workspace, msg=ANY)
    assert _subcommands(west) == ["init", "config", "update", "list"]
    init = _west_call(west, "init")
    assert init.args[0][init.args[0].index("init") :] == [
        "init",
        "-m",
        SOURCE.url,
        "-o=--depth=1",
        "--mr",
        VERSION,
        str(workspace),
    ]
    assert _project_filter(west) == "-hal_.*,+hal_a,+zephyr"
    update = _west_call(west, "update")
    assert update.args[0][-2:] == ["--narrow", "--fetch-opt=--depth=1"]
    assert update.kwargs == {"cwd": workspace, "stream_output": True}
    # Streamed, so the long clone's progress reaches the log
    assert init.kwargs["stream_output"] is True
    # Required projects are never checked by name
    assert _west_call(west, "list").args[0][-1:] == ["hal_a"]
    assert (workspace / ".west_projects").read_text(encoding="utf-8").split() == [
        "hal_a",
        "zephyr",
    ]


def test_install_of_only_required_projects_checks_no_names(
    workspace: Path, west: MagicMock
) -> None:
    _install(workspace, {"zephyr"})

    assert _subcommands(west) == ["init", "config", "update"]


def test_interrupted_download_resumes(workspace: Path, west: MagicMock) -> None:
    """A workspace left by a cut-short download is updated in place."""
    _mark_initialized(workspace)
    (workspace / ".resume_failed").touch()

    _install(workspace, {"zephyr"})

    west.rmdir.assert_not_called()
    assert _subcommands(west) == ["config", "update"]
    assert not (workspace / ".resume_failed").exists()


def test_failed_resume_keeps_the_download_once(
    workspace: Path, west: MagicMock
) -> None:
    """A first failed resume keeps what was fetched and is retried next build."""
    _mark_initialized(workspace)
    west.side_effect = None
    west.return_value = False

    with pytest.raises(EsphomeError, match="Can't resume the Test SDK v1.0.0"):
        _install(workspace, {"zephyr"})

    west.rmdir.assert_not_called()
    assert (workspace / ".resume_failed").exists()


def test_second_failed_resume_starts_over(workspace: Path, west: MagicMock) -> None:
    _mark_initialized(workspace)
    (workspace / ".resume_failed").touch()
    run = west.side_effect
    # The resumed update (second command) fails, the clean clone succeeds
    west.side_effect = lambda cmd, **kw: west.call_count != 2 and run(cmd, **kw)

    _install(workspace, {"zephyr"})

    west.rmdir.assert_called_once_with(workspace, msg=ANY)
    assert _subcommands(west) == ["config", "update", "init", "config", "update"]


def test_cut_short_init_starts_over(workspace: Path, west: MagicMock) -> None:
    """A .west without its config (init cut short) clones clean."""
    (workspace / ".west").mkdir(parents=True)

    _install(workspace, {"zephyr"})

    west.rmdir.assert_called_once_with(workspace, msg=ANY)
    assert _subcommands(west)[0] == "init"


def test_install_init_failure_raises(workspace: Path, west: MagicMock) -> None:
    west.side_effect = None
    west.return_value = False

    with pytest.raises(EsphomeError, match="Can't initialize Test SDK v1.0.0"):
        _install(workspace, {"zephyr"})


def test_install_update_failure_raises(workspace: Path, west: MagicMock) -> None:
    run = west.side_effect
    west.side_effect = lambda cmd, **kw: "update" not in cmd and run(cmd, **kw)

    with pytest.raises(EsphomeError, match="Can't update Test SDK v1.0.0"):
        _install(workspace, {"zephyr"})

    assert not (workspace / ".west_projects").exists()


def test_unknown_project_is_never_recorded(workspace: Path, west: MagicMock) -> None:
    """A filter naming a project the manifest lacks fetches nothing, so the
    names are checked once the update resolved the manifest."""
    run = west.side_effect
    west.side_effect = lambda cmd, **kw: "list" not in cmd and run(cmd, **kw)

    with pytest.raises(EsphomeError, match="west list failed .*no_such_project"):
        _install(workspace, {"zephyr", "no_such_project"})

    assert not (workspace / ".west_projects").exists()


def test_fetch_of_only_required_projects_reads_nothing(
    workspace: Path, west: MagicMock
) -> None:
    """Any install has the required projects; an unreadable stamp shows nothing looked."""
    _mark_finished(workspace, None)
    (workspace / ".west_projects").mkdir()

    _fetch(workspace, {"zephyr"})

    west.assert_not_called()


def test_fetch_gains_only_the_missing_project(workspace: Path, west: MagicMock) -> None:
    """The names are checked first; only the new project is updated, so a
    build running from the others is left alone."""
    _mark_finished(workspace, "hal_a\nzephyr")

    _fetch(workspace, {"zephyr", "hal_b"})

    assert _subcommands(west) == ["list", "config", "update"]
    assert _west_call(west, "list").args[0][-1:] == ["hal_b"]
    assert _project_filter(west) == "-hal_.*,+hal_a,+hal_b,+zephyr"
    assert _west_call(west, "update").args[0][-1] == "hal_b"
    assert (workspace / ".west_projects").read_text(encoding="utf-8").split() == [
        "hal_a",
        "hal_b",
        "zephyr",
    ]
    west.rmdir.assert_not_called()


@pytest.mark.parametrize(
    ("config", "stamp"),
    [
        pytest.param("", None, id="install_from_before_the_filter"),
        pytest.param(None, "hal_a\nzephyr", id="project_already_fetched"),
    ],
)
def test_fetch_with_the_project_only_checks_the_names(
    workspace: Path, west: MagicMock, config: str | None, stamp: str | None
) -> None:
    """An install without the stamp or a filter has every project; the names
    are still checked, so a mistake shows on every install alike."""
    _mark_finished(workspace, stamp)
    if config is not None:
        _mark_initialized(workspace, config)

    _fetch(workspace, {"zephyr", "hal_a"})

    assert _subcommands(west) == ["list"]


@pytest.mark.parametrize(
    "lose_config",
    [
        pytest.param(False, id="filtered_install_lost_its_stamp"),
        pytest.param(True, id="stamp_and_west_config_lost"),
    ],
)
def test_fetch_without_a_record_fetches_again(
    workspace: Path, west: MagicMock, lose_config: bool
) -> None:
    """Nothing says what the install has, so the wanted projects are fetched."""
    _mark_finished(workspace, None)
    if lose_config:
        (workspace / ".west" / "config").unlink()

    _fetch(workspace, {"zephyr", "hal_a"})

    assert _subcommands(west) == ["list", "config", "update"]
    assert _west_call(west, "update").args[0][-2:] == ["hal_a", "zephyr"]


@pytest.mark.parametrize(
    ("stamp", "restored"),
    [
        pytest.param("hal_a\nzephyr", "-hal_.*,+hal_a,+zephyr", id="stamp"),
        # An unknown installed set must not leave every project off
        pytest.param(None, "-hal_.*,+zephyr", id="lost_stamp"),
    ],
)
def test_failed_fetch_restores_the_filter(
    workspace: Path, west: MagicMock, stamp: str | None, restored: str
) -> None:
    """A failed fetch raises, keeps the stamp and puts the filter back to the
    stamp's projects and the required ones."""
    _mark_finished(workspace, stamp)
    run = west.side_effect
    west.side_effect = lambda cmd, **kw: "update" not in cmd and run(cmd, **kw)

    with pytest.raises(EsphomeError, match="Can't update Test SDK v1.0.0"):
        _fetch(workspace, {"zephyr", "hal_b"})

    assert _subcommands(west) == ["list", "config", "update", "config"]
    assert _project_filter(west) == restored
    if stamp is not None:
        assert (workspace / ".west_projects").read_text(encoding="utf-8") == stamp


def test_failed_filter_restore_is_logged(
    workspace: Path, west: MagicMock, caplog: pytest.LogCaptureFixture
) -> None:
    _mark_finished(workspace, "zephyr")
    # list and config succeed, update and the restoring config fail
    west.side_effect = [True, True, False, False]

    with pytest.raises(EsphomeError, match="Can't update"):
        _fetch(workspace, {"zephyr", "hal_b"})

    assert "Couldn't put the Test SDK v1.0.0 project filter back" in caplog.text


def test_fetch_of_an_unknown_project_changes_nothing(
    workspace: Path, west: MagicMock
) -> None:
    """On a finished install the names are checked first, so an unknown one
    costs no fetch and leaves the workspace as it was."""
    _mark_finished(workspace, "zephyr")
    west.side_effect = [False]

    with pytest.raises(EsphomeError, match="west list failed .*Test SDK v1.0.0"):
        _fetch(workspace, {"zephyr", "no_such_project"})

    assert _subcommands(west) == ["list"]
    assert (workspace / ".west_projects").read_text(encoding="utf-8") == "zephyr"


@pytest.fixture
def zephyr_reqs(tmp_path: Path) -> Path:
    reqs = tmp_path / "sdk" / "zephyr" / "scripts" / "requirements.txt"
    reqs.parent.mkdir(parents=True)
    reqs.write_text("pyelftools\n")
    (tmp_path / "penv").mkdir()
    return reqs


def _ensure_zephyr_requirements(tmp_path: Path, venv_rebuilt: bool = False) -> None:
    ensure_zephyr_requirements(tmp_path / "penv", tmp_path / "sdk", venv_rebuilt, "v1")


@pytest.mark.parametrize(
    ("marker_age", "venv_rebuilt", "installs"),
    [
        pytest.param(None, False, True, id="no_marker"),
        pytest.param(10, False, False, id="current"),
        pytest.param(-10, False, True, id="requirements_newer"),
        pytest.param(10, True, True, id="venv_rebuilt"),
    ],
)
def test_ensure_zephyr_requirements(
    tmp_path: Path,
    zephyr_reqs: Path,
    marker_age: int | None,
    venv_rebuilt: bool,
    installs: bool,
) -> None:
    """Installed when the venv was rebuilt, the marker is missing, or the
    workspace's requirements are newer than the marker."""
    marker = tmp_path / "penv" / ".zephyr_reqs_ready"
    if marker_age is not None:
        marker.touch()
        reqs_time = marker.stat().st_mtime - marker_age
        os.utime(zephyr_reqs, (reqs_time, reqs_time))

    with patch.object(framework_helpers, "run_command_ok", return_value=True) as pip:
        _ensure_zephyr_requirements(tmp_path, venv_rebuilt)

    assert pip.called is installs
    if installs:
        assert pip.call_args.args[0][-2:] == ["-r", str(zephyr_reqs)]
        assert marker.exists()


def test_ensure_zephyr_requirements_raises_when_pip_fails(
    tmp_path: Path, zephyr_reqs: Path
) -> None:
    with (
        patch.object(framework_helpers, "run_command_ok", return_value=False),
        pytest.raises(EsphomeError, match="Install Zephyr requirements for v1"),
    ):
        _ensure_zephyr_requirements(tmp_path)

    assert not (tmp_path / "penv" / ".zephyr_reqs_ready").exists()
