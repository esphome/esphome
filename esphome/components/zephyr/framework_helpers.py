"""Helpers shared by the Zephyr-based framework installers."""

from __future__ import annotations

import configparser
from dataclasses import dataclass
import hashlib
import logging
import os
from pathlib import Path
import shutil

from esphome.core import EsphomeError
from esphome.framework_helpers import (
    create_venv,
    get_python_env_executable_path,
    rmdir,
    run_command_ok,
)

_LOGGER = logging.getLogger(__name__)


def needs_venv_rebuild(
    env_python_path: Path, sentinel: Path, requirements_hash: str
) -> bool:
    """True when a penv must be (re)built.

    Rebuild when the interpreter is not a regular file, which covers a
    dangling symlink (a cached venv outliving a host interpreter upgrade)
    and a corrupt restore, or when the sentinel is missing or stale.
    """
    return (
        not env_python_path.is_file()
        or not sentinel.exists()
        or sentinel.read_text(encoding="utf-8") != requirements_hash
    )


_SITECUSTOMIZE = """\
import os, stat, shutil
_orig = shutil.rmtree
def _handler(func, path, exc):
    os.chmod(path, stat.S_IWRITE); func(path)
def _rmtree(path, ignore_errors=False, onerror=None, *, onexc=None, dir_fd=None):
    if onerror is None and onexc is None:
        onexc = _handler
    return _orig(path, ignore_errors=ignore_errors, onerror=onerror, onexc=onexc, dir_fd=dir_fd)
shutil.rmtree = _rmtree
"""


def install_sitecustomize(python_env_path: Path) -> None:
    """Patch shutil.rmtree inside the penv to handle read-only files.

    west init's shutil.move falls back to copytree+rmtree on Windows, and
    rmtree dies on the read-only .idx/.pack files git just wrote into
    manifest-tmp. Dropping a sitecustomize.py into the venv applies the
    same fix esphome.helpers.rmtree uses, but inside the subprocess.
    """
    if os.name != "nt":
        return
    site_packages = python_env_path / "Lib" / "site-packages"
    site_packages.mkdir(parents=True, exist_ok=True)
    (site_packages / "sitecustomize.py").write_text(_SITECUSTOMIZE, encoding="utf-8")


def patch_framework_file(path: Path, old: str, new: str) -> bool:
    """Replace ``old`` with ``new`` in a framework script, atomically and
    keeping the file mode (helpers.write_file would flatten it to 0o644).
    Returns False when nothing matched."""
    import tempfile

    content = path.read_text(encoding="utf-8")
    patched = content.replace(old, new)
    if patched == content:
        return False
    # Unique sibling tmp: the install lock is best effort, so two builds
    # may patch at once and a shared tmp name could rename a half
    # written file into place.
    fd, tmp_name = tempfile.mkstemp(dir=path.parent, suffix=".tmp")
    tmp = Path(tmp_name)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(patched)
        shutil.copymode(path, tmp)
        tmp.replace(path)
    except BaseException:
        tmp.unlink(missing_ok=True)
        raise
    return True


_WEST_BUILD_RELPATH = "        srcrel = os.path.relpath(self.source_dir)\n"
# Zephyr main's replacement, verbatim (zephyr commit dfbdca0e13)
_WEST_BUILD_RELPATH_SAFE = """        try:
            srcrel = pathlib.Path(self.source_dir).relative_to(pathlib.Path.cwd(), walk_up=True)
        except ValueError:
            srcrel = self.source_dir
"""


def patch_west_build_source_dir_relpath(framework_path: Path) -> None:
    # west build names the source dir relative to the working directory (the
    # SDK workspace) in two warnings; Python refuses that on Windows when the
    # project is on another drive, and west dies before building. Applies
    # Zephyr main's fix; SDKs that carry it have no line to patch.
    build = framework_path / "zephyr" / "scripts" / "west_commands" / "build.py"
    if build.exists():
        patch_framework_file(build, _WEST_BUILD_RELPATH, _WEST_BUILD_RELPATH_SAFE)


def patch_gen_defines_dts_path(framework_path: Path) -> None:
    # The absolute zephyr.dts.pre path in the header's top comment is
    # its only per device byte and blocks ccache sharing; emit the
    # basename. Upstream candidate.
    gen_defines = framework_path / "zephyr" / "scripts" / "dts" / "gen_defines.py"
    if not gen_defines.exists():
        return
    if patch_framework_file(
        gen_defines, "  {edt.dts_path}", "  {os.path.basename(edt.dts_path)}"
    ):
        return
    if "{os.path.basename(edt.dts_path)}" not in gen_defines.read_text(
        encoding="utf-8"
    ):
        # Upstream reformatted the comment; sharing silently degrading
        # would be invisible, so say it out loud.
        _LOGGER.warning(
            "gen_defines.py no longer matches; the devicetree header "
            "stays per device and ccache sharing between devices degrades"
        )


def ensure_venv(python_env_path: Path, requirements: Path, label: str) -> bool:
    """Build the penv from ``requirements`` unless it is current.

    Returns True when it was (re)built.
    """
    env_python_path = get_python_env_executable_path(python_env_path, "python")
    sentinel = python_env_path / ".ready"
    requirements_hash = hashlib.sha256(requirements.read_bytes()).hexdigest()
    if not needs_venv_rebuild(env_python_path, sentinel, requirements_hash):
        return False
    rmdir(python_env_path, msg=f"Clean up {label} Python environment")

    create_venv(python_env_path, msg=label)

    install_sitecustomize(python_env_path)

    _LOGGER.info("Installing requirements ...")
    cmd = [
        str(env_python_path),
        "-m",
        "pip",
        "install",
        "-r",
        str(requirements),
    ]
    if not run_command_ok(cmd):
        raise EsphomeError(
            f"Install requirements for {label} Python environment failure"
        )
    sentinel.write_text(requirements_hash, encoding="utf-8")
    return True


@dataclass(frozen=True)
class FrameworkSource:
    """Where a framework comes from, and which of its projects a workspace fetches."""

    # The framework's name in messages
    name: str
    # The repository it is cloned from
    url: str
    # Projects (patterns) fetched only when a build needs them
    optional: tuple[str, ...]
    # Projects every build fetches
    required: frozenset[str]


# The projects a finished install fetched
_WEST_PROJECTS_FILE: str = ".west_projects"


def _set_project_filter(
    env_python_path: Path,
    framework_path: Path,
    source: FrameworkSource,
    projects: set[str],
) -> bool:
    project_filter = ",".join(
        [*(f"-{p}" for p in source.optional), *(f"+{p}" for p in sorted(projects))]
    )
    # "--" keeps west from reading the leading "-" as an option
    cmd = [str(env_python_path), "-m", "west", "config", "manifest.project-filter"]
    return run_command_ok([*cmd, "--", project_filter], cwd=framework_path)


def _check_west_projects(
    env_python_path: Path,
    framework_path: Path,
    source: FrameworkSource,
    version: str,
    projects: set[str],
) -> None:
    """Raise when the manifest lacks one of ``projects``.

    west update quietly skips an unknown name in the filter, west list fails on it.
    Needs the workspace updated once, since a manifest may import others.
    """
    names = sorted(projects)
    cmd = [str(env_python_path), "-m", "west", "list", "-f", "{name}", *names]
    if not run_command_ok(cmd, cwd=framework_path):
        raise EsphomeError(
            f"west list failed for the requested {source.name} {version} projects "
            f"({', '.join(names)}); a project the manifest does not have is the "
            "usual cause, see west's output above"
        )


def _west_update(
    env_python_path: Path,
    framework_path: Path,
    source: FrameworkSource,
    version: str,
    projects: set[str],
    *,
    only: set[str] | None = None,
) -> bool:
    """Fetch ``projects``, or just ``only`` of them; False when the fetch fails.

    ``only`` is for a finished workspace, whose names are checked already.
    """
    if not _set_project_filter(env_python_path, framework_path, source, projects):
        return False
    cmd = [
        str(env_python_path),
        "-m",
        "west",
        "update",
        "--narrow",
        "--fetch-opt=--depth=1",
        *sorted(only or ()),
    ]
    # Streamed so the clone's progress reaches the log
    if not run_command_ok(cmd, cwd=framework_path, stream_output=True):
        return False
    if only is None:
        _check_west_projects(env_python_path, framework_path, source, version, projects)
    (framework_path / _WEST_PROJECTS_FILE).write_text(
        "\n".join(sorted(projects)), encoding="utf-8"
    )
    return True


def _installed_west_projects(framework_path: Path) -> set[str] | None:
    """The projects a finished install fetched; None when it has every project."""
    try:
        stamp = (framework_path / _WEST_PROJECTS_FILE).read_text(encoding="utf-8")
    except FileNotFoundError:
        pass
    else:
        return set(stamp.split())
    # No stamp: an install from before the filter has every project, a filtered
    # one that lost its stamp fetches again
    config = configparser.ConfigParser()
    if not config.read(framework_path / ".west" / "config", encoding="utf-8"):
        return set()
    if config.has_option("manifest", "project-filter"):
        return set()
    return None


def _restore_project_filter(
    env_python_path: Path,
    framework_path: Path,
    source: FrameworkSource,
    version: str,
    installed: set[str],
) -> None:
    """Put the filter back to the stamp's projects; the required ones always stay in."""
    projects = installed | source.required
    if not _set_project_filter(env_python_path, framework_path, source, projects):
        _LOGGER.warning(
            "Couldn't put the %s %s project filter back; "
            "the next build that fetches a project sets it again",
            source.name,
            version,
        )


def fetch_missing_west_projects(
    env_python_path: Path,
    framework_path: Path,
    source: FrameworkSource,
    version: str,
    projects: set[str],
) -> None:
    """Fetch the wanted projects a finished install lacks; it only ever gains.

    Only the missing projects are updated, so a build running from the others
    is left alone. Call under the workspace's install lock.
    """
    if projects <= source.required:
        return
    installed = _installed_west_projects(framework_path)
    # Before the fetch, so an unknown name costs nothing on any install
    _check_west_projects(env_python_path, framework_path, source, version, projects)
    if installed is None or not (missing := projects - installed):
        return
    _LOGGER.info(
        "Fetching %s %s projects: %s",
        source.name,
        version,
        ", ".join(sorted(missing)),
    )
    if not _west_update(
        env_python_path,
        framework_path,
        source,
        version,
        installed | projects,
        only=missing,
    ):
        _restore_project_filter(
            env_python_path, framework_path, source, version, installed
        )
        raise EsphomeError(f"Can't update {source.name} {version}")


def install_framework(
    env_python_path: Path,
    framework_path: Path,
    source: FrameworkSource,
    version: str,
    projects: set[str],
) -> None:
    """Clone the SDK into ``framework_path`` with west, fetching ``projects``.

    A download cut short after ``west init`` leaves the workspace behind;
    rerunning ``west update`` there only fetches what is missing, so it resumes
    instead of cloning again. A resume that fails keeps what was fetched (a
    flaky network is the likely cause) and is retried on the next build; only
    a second failure in a row starts over clean. Call under the workspace's
    install lock.
    """
    name = source.name
    resume_failed = framework_path / ".resume_failed"
    # Resume only a workspace whose ``west init`` finished (it writes the
    # config last). ``.ready`` with missing requirements is a damaged install,
    # not an interrupted one, so it goes the clean way.
    initialized = (framework_path / ".west" / "config").is_file()
    if initialized and not (framework_path / ".ready").exists():
        _LOGGER.info("Resuming the %s %s download ...", name, version)
        if _west_update(env_python_path, framework_path, source, version, projects):
            resume_failed.unlink(missing_ok=True)
            return
        if not resume_failed.exists():
            resume_failed.touch()
            raise EsphomeError(
                f"Can't resume the {name} {version} download; the next build retries it"
            )
        _LOGGER.warning("Resuming failed again; downloading %s %s anew", name, version)
    rmdir(framework_path, msg=f"Clean up {version} framework environment")
    _LOGGER.info("Initializing %s %s ...", name, version)
    cmd = [
        str(env_python_path),
        "-m",
        "west",
        "init",
        "-m",
        source.url,
        "-o=--depth=1",
        "--mr",
        version,
        str(framework_path),
    ]
    if not run_command_ok(cmd, stream_output=True):
        raise EsphomeError(f"Can't initialize {name} {version}")
    _LOGGER.info("Updating %s %s (this may take a while) ...", name, version)
    if not _west_update(env_python_path, framework_path, source, version, projects):
        raise EsphomeError(f"Can't update {name} {version}")


def ensure_zephyr_requirements(
    python_env_path: Path,
    framework_path: Path,
    venv_rebuilt: bool,
    label: str,
    marker_name: str,
) -> None:
    """Install the workspace's Zephyr Python requirements into the penv unless current.

    ``marker_name`` is the file in the penv that records the install, so a venv
    rebuild removes it.
    """
    zephyr_reqs = framework_path / "zephyr" / "scripts" / "requirements.txt"
    sentinel = python_env_path / marker_name
    if (
        not venv_rebuilt
        and sentinel.exists()
        and zephyr_reqs.stat().st_mtime <= sentinel.stat().st_mtime
    ):
        return
    _LOGGER.info("Installing Zephyr requirements ...")
    env_python_path = get_python_env_executable_path(python_env_path, "python")
    cmd = [str(env_python_path), "-m", "pip", "install", "-r", str(zephyr_reqs)]
    if not run_command_ok(cmd):
        raise EsphomeError(f"Install Zephyr requirements for {label} failure")
    sentinel.touch()
