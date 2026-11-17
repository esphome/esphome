"""Helpers shared by the Zephyr-based framework installers."""

from __future__ import annotations

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
