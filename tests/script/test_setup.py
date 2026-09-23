"""Tests for script/setup.py."""

import importlib.util
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import runpy
import sys
from types import ModuleType
from unittest.mock import Mock, call, patch

import pytest

_SCRIPT = Path(__file__).parents[2] / "script" / "setup.py"


def _load_module() -> ModuleType:
    spec = importlib.util.spec_from_file_location("script_setup", _SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.fixture
def script_setup() -> ModuleType:
    """Fresh import of script/setup.py, isolated from other tests."""
    return _load_module()


# --- bin_dir / venv_python / activate_hint -----------------------------------


def test_bin_dir_matches_host_layout(script_setup: ModuleType, tmp_path: Path) -> None:
    """The venv scheme resolves to Scripts on Windows and bin everywhere else."""
    expected = "Scripts" if os.name == "nt" else "bin"
    assert script_setup.bin_dir(tmp_path) == tmp_path / expected


# Both flavours are exercised on every host. Pure paths are used because a real
# Path refuses to change flavour: PosixPath cannot be built on Windows, and
# WindowsPath cannot be built on Unix.


def test_venv_python_posix(script_setup: ModuleType, tmp_path: Path) -> None:
    with (
        patch.object(
            script_setup, "bin_dir", return_value=PurePosixPath("/x/venv/bin")
        ),
        patch.object(script_setup.os, "name", "posix"),
    ):
        result = script_setup.venv_python(tmp_path)
    assert result == PurePosixPath("/x/venv/bin/python")


def test_venv_python_nt(script_setup: ModuleType, tmp_path: Path) -> None:
    with (
        patch.object(
            script_setup, "bin_dir", return_value=PureWindowsPath(r"C:\x\venv\Scripts")
        ),
        patch.object(script_setup.os, "name", "nt"),
    ):
        result = script_setup.venv_python(tmp_path)
    assert result == PureWindowsPath(r"C:\x\venv\Scripts\python.exe")


def test_activate_hint_posix(script_setup: ModuleType) -> None:
    with (
        patch.object(script_setup, "ROOT", PurePosixPath("/x")),
        patch.object(
            script_setup, "bin_dir", return_value=PurePosixPath("/x/venv/bin")
        ),
        patch.object(script_setup.os, "name", "posix"),
    ):
        hint = script_setup.activate_hint()
    assert hint == "source venv/bin/activate"


def test_activate_hint_nt(script_setup: ModuleType) -> None:
    with (
        patch.object(script_setup, "ROOT", PureWindowsPath(r"C:\x")),
        patch.object(
            script_setup, "bin_dir", return_value=PureWindowsPath(r"C:\x\venv\Scripts")
        ),
        patch.object(script_setup.os, "name", "nt"),
    ):
        hint = script_setup.activate_hint()
    # The nt branch returns str(activate) as-is, skipping the "source " prefix.
    assert hint == r"venv\Scripts\activate"


# --- run -----------------------------------------------------------------


def test_run_success(script_setup: ModuleType) -> None:
    with patch.object(
        script_setup.subprocess, "run", return_value=Mock(returncode=0)
    ) as mock_run:
        script_setup.run(["echo", "hi"])
    mock_run.assert_called_once_with(
        ["echo", "hi"], cwd=script_setup.ROOT, env=None, check=False
    )


def test_run_failure_raises_system_exit_with_code(
    script_setup: ModuleType, capsys: pytest.CaptureFixture[str]
) -> None:
    with (
        patch.object(script_setup.subprocess, "run", return_value=Mock(returncode=7)),
        pytest.raises(SystemExit) as excinfo,
    ):
        script_setup.run(["false"])
    assert excinfo.value.code == 7
    assert "Failed with exit code 7: false" in capsys.readouterr().err


# --- git_output ------------------------------------------------------------


def test_git_output_success_strips_stdout(script_setup: ModuleType) -> None:
    with patch.object(
        script_setup.subprocess,
        "run",
        return_value=Mock(returncode=0, stdout="  /repo/.git \n"),
    ) as mock_run:
        result = script_setup.git_output("rev-parse", "--absolute-git-dir")
    assert result == "/repo/.git"
    mock_run.assert_called_once_with(
        ["git", "rev-parse", "--absolute-git-dir"],
        cwd=script_setup.ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def test_git_output_nonzero_returncode_is_empty(script_setup: ModuleType) -> None:
    with patch.object(
        script_setup.subprocess,
        "run",
        return_value=Mock(returncode=1, stdout="whatever"),
    ):
        assert script_setup.git_output("status") == ""


def test_git_output_oserror_is_empty(script_setup: ModuleType) -> None:
    with patch.object(script_setup.subprocess, "run", side_effect=OSError("no git")):
        assert script_setup.git_output("status") == ""


# --- create_venv -----------------------------------------------------------


def test_create_venv_uses_uv_when_present(
    script_setup: ModuleType, tmp_path: Path
) -> None:
    venv = tmp_path / "venv"
    with (
        patch.object(script_setup.shutil, "which", return_value="/usr/bin/uv"),
        patch.object(
            script_setup.subprocess, "run", return_value=Mock(returncode=0)
        ) as mock_run,
    ):
        script_setup.create_venv(venv)
    mock_run.assert_called_once_with(
        ["/usr/bin/uv", "venv", "--clear", "--seed", str(venv)],
        cwd=script_setup.ROOT,
        env=None,
        check=False,
    )


def test_create_venv_falls_back_to_venv_module(
    script_setup: ModuleType, tmp_path: Path
) -> None:
    venv = tmp_path / "venv"
    with (
        patch.object(script_setup.shutil, "which", return_value=None),
        patch.object(
            script_setup.subprocess, "run", return_value=Mock(returncode=0)
        ) as mock_run,
    ):
        script_setup.create_venv(venv)
    mock_run.assert_called_once_with(
        [sys.executable, "-m", "venv", "--clear", str(venv)],
        cwd=script_setup.ROOT,
        env=None,
        check=False,
    )


# --- venv_environment --------------------------------------------------------


def test_venv_environment_sets_virtual_env_and_prepends_path(
    script_setup: ModuleType, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    venv = tmp_path / "venv"
    monkeypatch.setenv("PYTHONHOME", "/somewhere")
    monkeypatch.setenv("PATH", "/usr/bin:/bin")
    env = script_setup.venv_environment(venv)
    assert env["VIRTUAL_ENV"] == str(venv)
    assert "PYTHONHOME" not in env
    expected_prefix = str(script_setup.bin_dir(venv)) + os.pathsep
    assert env["PATH"] == expected_prefix + "/usr/bin:/bin"


def test_venv_environment_path_fallback_when_unset(
    script_setup: ModuleType, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    venv = tmp_path / "venv"
    monkeypatch.delenv("PATH", raising=False)
    env = script_setup.venv_environment(venv)
    # No trailing separator: an empty PATH entry means "search the cwd".
    assert env["PATH"] == str(script_setup.bin_dir(venv))


# --- find_uv -----------------------------------------------------------------


def test_find_uv_found_immediately(script_setup: ModuleType, tmp_path: Path) -> None:
    venv = tmp_path / "venv"
    env = {"PATH": "/usr/bin"}
    with (
        patch.object(script_setup.shutil, "which", return_value="/usr/bin/uv"),
        patch.object(script_setup.subprocess, "run") as mock_run,
    ):
        result = script_setup.find_uv(venv, env)
    assert result == "/usr/bin/uv"
    mock_run.assert_not_called()


def test_find_uv_installed_then_found(script_setup: ModuleType, tmp_path: Path) -> None:
    venv = tmp_path / "venv"
    env = {"PATH": "/usr/bin"}
    with (
        patch.object(script_setup.shutil, "which", side_effect=[None, "/usr/bin/uv"]),
        patch.object(
            script_setup.subprocess, "run", return_value=Mock(returncode=0)
        ) as mock_run,
    ):
        result = script_setup.find_uv(venv, env)
    assert result == "/usr/bin/uv"
    mock_run.assert_called_once_with(
        [str(script_setup.venv_python(venv)), "-m", "pip", "install", "uv"],
        cwd=script_setup.ROOT,
        env=env,
        check=False,
    )


def test_find_uv_still_missing_raises_system_exit(
    script_setup: ModuleType, tmp_path: Path
) -> None:
    venv = tmp_path / "venv"
    env = {"PATH": "/usr/bin"}
    with (
        patch.object(script_setup.shutil, "which", side_effect=[None, None]),
        patch.object(script_setup.subprocess, "run", return_value=Mock(returncode=0)),
        pytest.raises(SystemExit, match="uv could not be installed"),
    ):
        script_setup.find_uv(venv, env)


# --- install_dependencies -----------------------------------------------------


def test_install_dependencies_installs_setuptools_then_project(
    script_setup: ModuleType, tmp_path: Path
) -> None:
    venv = tmp_path / "venv"
    env = {"PATH": "/usr/bin"}
    with (
        patch.object(script_setup.shutil, "which", return_value="/usr/bin/uv"),
        patch.object(
            script_setup.subprocess, "run", return_value=Mock(returncode=0)
        ) as mock_run,
    ):
        script_setup.install_dependencies(venv, env)
    assert mock_run.call_args_list == [
        call(
            ["/usr/bin/uv", "pip", "install", "setuptools", "wheel"],
            cwd=script_setup.ROOT,
            env=env,
            check=False,
        ),
        call(
            [
                "/usr/bin/uv",
                "pip",
                "install",
                "-e",
                ".[dev,test]",
                "--config-settings",
                "editable_mode=compat",
            ],
            cwd=script_setup.ROOT,
            env=env,
            check=False,
        ),
    ]


# --- install_git_hooks ---------------------------------------------------------


def _fake_git_output(git_dir: str, common_dir: str):
    def _run(*args: str) -> str:
        if "--absolute-git-dir" in args:
            return git_dir
        return common_dir

    return _run


def test_install_git_hooks_returns_early_when_git_dir_empty(
    script_setup: ModuleType,
) -> None:
    env = {"PATH": "/usr/bin"}
    with (
        patch.object(
            script_setup, "git_output", side_effect=_fake_git_output("", "/repo/.git")
        ),
        patch.object(script_setup.subprocess, "run") as mock_run,
    ):
        script_setup.install_git_hooks(env)
    mock_run.assert_not_called()


def test_install_git_hooks_returns_early_when_common_dir_empty(
    script_setup: ModuleType,
) -> None:
    env = {"PATH": "/usr/bin"}
    with (
        patch.object(
            script_setup, "git_output", side_effect=_fake_git_output("/repo/.git", "")
        ),
        patch.object(script_setup.subprocess, "run") as mock_run,
    ):
        script_setup.install_git_hooks(env)
    mock_run.assert_not_called()


def test_install_git_hooks_returns_early_for_worktree(
    script_setup: ModuleType,
) -> None:
    """A worktree's git-dir differs from the shared common-dir."""
    env = {"PATH": "/usr/bin"}
    with (
        patch.object(
            script_setup,
            "git_output",
            side_effect=_fake_git_output("/repo/.git/worktrees/wt", "/repo/.git"),
        ),
        patch.object(script_setup.subprocess, "run") as mock_run,
    ):
        script_setup.install_git_hooks(env)
    mock_run.assert_not_called()


def test_install_git_hooks_missing_prek_raises_system_exit(
    script_setup: ModuleType,
) -> None:
    env = {"PATH": "/usr/bin"}
    with (
        patch.object(
            script_setup,
            "git_output",
            side_effect=_fake_git_output("/repo/.git", "/repo/.git"),
        ),
        patch.object(script_setup.shutil, "which", return_value=None),
        patch.object(script_setup.subprocess, "run") as mock_run,
        pytest.raises(SystemExit, match="prek was not installed"),
    ):
        script_setup.install_git_hooks(env)
    mock_run.assert_not_called()


def test_install_git_hooks_happy_path_installs_hook(
    script_setup: ModuleType, tmp_path: Path
) -> None:
    env = {"PATH": "/usr/bin"}
    common_dir = tmp_path / "repo" / ".git"
    hooks_dir = common_dir / "hooks"
    hooks_dir.mkdir(parents=True)
    source_hook = tmp_path / "post-checkout"
    source_hook.write_text("#!/bin/sh\necho post-checkout\n")

    with (
        patch.object(script_setup, "POST_CHECKOUT_HOOK", source_hook),
        patch.object(
            script_setup,
            "git_output",
            side_effect=_fake_git_output(str(common_dir), str(common_dir)),
        ),
        patch.object(script_setup.shutil, "which", return_value="/usr/bin/prek"),
        patch.object(
            script_setup.subprocess, "run", return_value=Mock(returncode=0)
        ) as mock_run,
    ):
        script_setup.install_git_hooks(env)

    mock_run.assert_called_once_with(
        ["/usr/bin/prek", "install", "--overwrite"],
        cwd=script_setup.ROOT,
        env=env,
        check=False,
    )
    installed = hooks_dir / "post-checkout"
    assert installed.read_text() == source_hook.read_text()
    if os.name != "nt":
        # Windows has no POSIX permission bits for chmod to set.
        assert (installed.stat().st_mode & 0o777) == 0o755


def test_install_git_hooks_skips_copy_when_hooks_dir_missing(
    script_setup: ModuleType, tmp_path: Path
) -> None:
    """The prek install still runs when the hooks directory does not exist."""
    env = {"PATH": "/usr/bin"}
    common_dir = tmp_path / "repo" / ".git"
    common_dir.mkdir(parents=True)  # no "hooks" subdirectory created

    with (
        patch.object(
            script_setup,
            "git_output",
            side_effect=_fake_git_output(str(common_dir), str(common_dir)),
        ),
        patch.object(script_setup.shutil, "which", return_value="/usr/bin/prek"),
        patch.object(
            script_setup.subprocess, "run", return_value=Mock(returncode=0)
        ) as mock_run,
    ):
        script_setup.install_git_hooks(env)

    mock_run.assert_called_once()
    assert not (common_dir / "hooks").exists()


# --- report ------------------------------------------------------------------


def test_report_active_state(
    script_setup: ModuleType, capsys: pytest.CaptureFixture
) -> None:
    venv = Path("/opt/esphome-venv")
    script_setup.report(script_setup.VENV_ACTIVE, venv)
    out = capsys.readouterr().out
    assert "Dependencies installed into the active virtual environment:" in out
    assert str(venv) in out
    assert "is already active in this shell" in out


def test_report_reused_state(
    script_setup: ModuleType, capsys: pytest.CaptureFixture
) -> None:
    script_setup.report(script_setup.VENV_REUSED, script_setup.DEFAULT_VENV)
    out = capsys.readouterr().out
    assert "Dependencies updated in the existing ./venv" in out


def test_report_created_state(
    script_setup: ModuleType, capsys: pytest.CaptureFixture
) -> None:
    script_setup.report(script_setup.VENV_CREATED, script_setup.DEFAULT_VENV)
    out = capsys.readouterr().out
    assert "Virtual environment created at ./venv" in out


# --- main --------------------------------------------------------------------


def test_main_raises_system_exit_when_python_too_old(
    script_setup: ModuleType,
) -> None:
    with (
        patch.object(script_setup.sys, "version_info", (3, 11, 5)),
        pytest.raises(SystemExit, match="ESPHome needs Python 3.12"),
    ):
        script_setup.main()


def test_main_uses_active_virtual_env(
    script_setup: ModuleType, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    active_venv = tmp_path / "active-venv"
    monkeypatch.setenv("VIRTUAL_ENV", str(active_venv))
    with (
        patch.object(script_setup, "ROOT", tmp_path),
        patch.object(script_setup, "create_venv") as mock_create_venv,
        patch.object(script_setup, "install_dependencies") as mock_install_deps,
        patch.object(script_setup, "install_git_hooks") as mock_install_hooks,
        patch.object(script_setup, "report") as mock_report,
    ):
        script_setup.main()
    mock_create_venv.assert_not_called()
    mock_install_deps.assert_called_once()
    mock_install_hooks.assert_called_once()
    mock_report.assert_called_once_with(script_setup.VENV_ACTIVE, active_venv)
    assert (tmp_path / ".temp").is_dir()


def test_main_reuses_existing_venv(
    script_setup: ModuleType, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.delenv("VIRTUAL_ENV", raising=False)
    default_venv = tmp_path / "venv"
    python_path = script_setup.venv_python(default_venv)
    python_path.parent.mkdir(parents=True)
    python_path.touch()

    with (
        patch.object(script_setup, "ROOT", tmp_path),
        patch.object(script_setup, "DEFAULT_VENV", default_venv),
        patch.object(script_setup, "create_venv") as mock_create_venv,
        patch.object(script_setup, "install_dependencies") as mock_install_deps,
        patch.object(script_setup, "install_git_hooks") as mock_install_hooks,
        patch.object(script_setup, "report") as mock_report,
    ):
        script_setup.main()
    mock_create_venv.assert_not_called()
    mock_install_deps.assert_called_once()
    mock_install_hooks.assert_called_once()
    mock_report.assert_called_once_with(script_setup.VENV_REUSED, default_venv)
    assert (tmp_path / ".temp").is_dir()


def test_main_creates_new_venv(
    script_setup: ModuleType, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.delenv("VIRTUAL_ENV", raising=False)
    default_venv = tmp_path / "venv"  # does not exist yet

    with (
        patch.object(script_setup, "ROOT", tmp_path),
        patch.object(script_setup, "DEFAULT_VENV", default_venv),
        patch.object(script_setup, "create_venv") as mock_create_venv,
        patch.object(script_setup, "install_dependencies") as mock_install_deps,
        patch.object(script_setup, "install_git_hooks") as mock_install_hooks,
        patch.object(script_setup, "report") as mock_report,
    ):
        script_setup.main()
    mock_create_venv.assert_called_once_with(default_venv)
    mock_install_deps.assert_called_once()
    mock_install_hooks.assert_called_once()
    mock_report.assert_called_once_with(script_setup.VENV_CREATED, default_venv)
    assert (tmp_path / ".temp").is_dir()


def test_run_as_script_calls_main(tmp_path: Path) -> None:
    """The __main__ guard runs the whole flow, with every side effect stubbed."""
    completed = Mock(returncode=0, stdout="")
    with (
        patch("subprocess.run", return_value=completed) as mock_run,
        patch("shutil.which", return_value="/usr/bin/uv"),
        patch("pathlib.Path.mkdir") as mock_mkdir,
        patch.dict(os.environ, {"VIRTUAL_ENV": str(tmp_path / "env")}),
    ):
        runpy.run_path(str(_SCRIPT), run_name="__main__")

    # The dependency install ran, and git reported no hooks directory to touch.
    assert mock_run.called
    mock_mkdir.assert_called_once_with(exist_ok=True)
