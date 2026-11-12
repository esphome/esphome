"""Helpers shared by the Zephyr-based framework installers."""

from __future__ import annotations

import os
from pathlib import Path


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
