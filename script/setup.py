#!/usr/bin/env python3
"""Set up the ESPHome development environment.

Shared implementation behind script/setup and script/setup.bat, so the Unix and
Windows entry points cannot drift apart. Uses only the standard library: it runs
before any dependency has been installed.
"""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import sysconfig

MIN_PYTHON = (3, 12)

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_VENV = ROOT / "venv"
POST_CHECKOUT_HOOK = ROOT / "script" / "git-hooks" / "post-checkout"

# State of the environment the dependencies end up in, used for the closing
# message.
VENV_ACTIVE = "active"
VENV_REUSED = "reused"
VENV_CREATED = "created"


def bin_dir(venv: Path) -> Path:
    """Return the directory holding a virtual environment's executables.

    The "venv" scheme resolves to bin on Unix and Scripts on Windows, so the
    layout does not have to be hardcoded here.
    """
    base = str(venv)
    return Path(
        sysconfig.get_path("scripts", "venv", vars={"base": base, "platbase": base})
    )


def venv_python(venv: Path) -> Path:
    """Return the path to a virtual environment's interpreter."""
    name = "python.exe" if os.name == "nt" else "python"
    return bin_dir(venv) / name


def run(command: list[str], env: dict[str, str] | None = None) -> None:
    """Run a command, aborting the whole script if it fails."""
    print(f"+ {' '.join(command)}", flush=True)
    result = subprocess.run(command, cwd=ROOT, env=env, check=False)
    if result.returncode != 0:
        # Some tools fail without printing anything, so name the step that broke.
        print(
            f"Failed with exit code {result.returncode}: {command[0]}", file=sys.stderr
        )
        raise SystemExit(result.returncode)


def git_output(*args: str) -> str:
    """Return the trimmed output of a git command, or "" if it cannot be run."""
    try:
        result = subprocess.run(
            ["git", *args], cwd=ROOT, capture_output=True, text=True, check=False
        )
    except OSError:
        # Git is not required to install the dependencies, only to install hooks.
        return ""
    if result.returncode != 0:
        return ""
    return result.stdout.strip()


def create_venv(venv: Path) -> None:
    """Create a virtual environment, replacing anything already at the path."""
    # --clear replaces a partial environment left behind by an interrupted run.
    if (uv := shutil.which("uv")) is not None:
        run([uv, "venv", "--clear", "--seed", str(venv)])
    else:
        run([sys.executable, "-m", "venv", "--clear", str(venv)])


def venv_environment(venv: Path) -> dict[str, str]:
    """Return the environment child processes need to target a virtual env.

    Equivalent to sourcing the environment's activate script: tools such as uv
    and prek pick the environment up from VIRTUAL_ENV and PATH.
    """
    env = dict(os.environ)
    env["VIRTUAL_ENV"] = str(venv)
    env.pop("PYTHONHOME", None)
    env["PATH"] = os.pathsep.join([str(bin_dir(venv)), env.get("PATH", "")])
    return env


def find_uv(venv: Path, env: dict[str, str]) -> str:
    """Return the path to uv, installing it into the environment if needed."""
    if (uv := shutil.which("uv", path=env["PATH"])) is not None:
        return uv
    run([str(venv_python(venv)), "-m", "pip", "install", "uv"], env=env)
    if (uv := shutil.which("uv", path=env["PATH"])) is not None:
        return uv
    raise SystemExit("uv could not be installed, aborting.")


def install_dependencies(venv: Path, env: dict[str, str]) -> None:
    """Install ESPHome and its development dependencies into the environment."""
    uv = find_uv(venv, env)
    run([uv, "pip", "install", "setuptools", "wheel"], env=env)
    # The dev and test extras pull in requirements_dev.txt and
    # requirements_test.txt, and the package itself pulls in requirements.txt,
    # so this single install covers every requirements file.
    run(
        [
            uv,
            "pip",
            "install",
            "-e",
            ".[dev,test]",
            "--config-settings",
            "editable_mode=compat",
        ],
        env=env,
    )


def install_git_hooks(env: dict[str, str]) -> None:
    """Install the git hooks, but only when run from the main checkout.

    A worktree shares one git hooks directory with the main checkout it was
    created from. Installing from a worktree would point the shared hook at that
    worktree's virtual environment, breaking it for everyone once the worktree is
    removed.
    """
    git_dir = git_output("rev-parse", "--absolute-git-dir")
    common_dir = git_output("rev-parse", "--path-format=absolute", "--git-common-dir")
    if not git_dir or not common_dir or Path(git_dir) != Path(common_dir):
        return

    prek = shutil.which("prek", path=env["PATH"])
    if prek is None:
        raise SystemExit("prek was not installed, aborting.")
    # --overwrite replaces any hook already in place. Without it, prek finds a
    # previously installed pre-commit hook, moves it aside to
    # .git/hooks/pre-commit.legacy and keeps calling it, so every commit would
    # run both tools.
    run([prek, "install", "--overwrite"], env=env)

    # Prepares the virtual environment for new checkouts and worktrees. Installed
    # once here, it covers every worktree created from this checkout.
    hooks_dir = Path(common_dir) / "hooks"
    if hooks_dir.is_dir():
        installed = hooks_dir / "post-checkout"
        shutil.copyfile(POST_CHECKOUT_HOOK, installed)
        installed.chmod(0o755)


def activate_hint() -> str:
    """Return the command that activates the environment this script creates."""
    activate = bin_dir(DEFAULT_VENV).relative_to(ROOT) / "activate"
    if os.name == "nt":
        return str(activate)
    return f"source {activate.as_posix()}"


def report(state: str, venv: Path) -> None:
    """Print the closing message for the environment that was set up."""
    location = f"./{DEFAULT_VENV.name}"
    print()
    print()
    if state == VENV_ACTIVE:
        print("Dependencies installed into the active virtual environment:")
        print(f"  {venv}")
        print(
            f"It is already active in this shell, so no '{activate_hint()}' is needed."
        )
    elif state == VENV_REUSED:
        print(
            f"Dependencies updated in the existing {location}. "
            f"Run '{activate_hint()}' to use it."
        )
    else:
        print(
            f"Virtual environment created at {location}. "
            f"Run '{activate_hint()}' to use it."
        )


def main() -> None:
    """Set up the development environment."""
    if sys.version_info < MIN_PYTHON:
        raise SystemExit(
            f"ESPHome needs Python {MIN_PYTHON[0]}.{MIN_PYTHON[1]} or newer, "
            f"but this is Python {sys.version.split()[0]}."
        )

    # A virtual environment that is already active (for example the
    # devcontainer's pre-provisioned esphome-venv) is installed into rather than
    # creating a ./venv in the workspace.
    if active := os.environ.get("VIRTUAL_ENV"):
        state, venv = VENV_ACTIVE, Path(active)
    elif venv_python(DEFAULT_VENV).is_file():
        # Reuse the environment from an earlier run, so this script can be run
        # again at any time to pick up dependency changes.
        state, venv = VENV_REUSED, DEFAULT_VENV
    else:
        state, venv = VENV_CREATED, DEFAULT_VENV
        create_venv(venv)

    env = venv_environment(venv)
    install_dependencies(venv, env)
    install_git_hooks(env)
    (ROOT / ".temp").mkdir(exist_ok=True)
    report(state, venv)


if __name__ == "__main__":
    main()
