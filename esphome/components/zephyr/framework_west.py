import hashlib
import logging
import os
from pathlib import Path
import re
import subprocess
import time

from esphome.const import CONF_PATH, CONF_REF, CONF_TYPE, CONF_URL, TYPE_LOCAL
from esphome.core import CORE, EsphomeError, TimePeriodSeconds
from esphome.framework_helpers import (
    create_venv,
    get_python_env_executable_path,
    rmdir,
    run_command_ok,
)
from esphome.types import ConfigType

from .variants import ZephyrSDK

_LOGGER = logging.getLogger(__name__)

_TOOLS_SUBDIR = "sdk-zephyr"

# TimePeriodSeconds(seconds=-1) means "never refresh", mirroring esphome.git's NEVER_REFRESH.
NEVER_REFRESH = TimePeriodSeconds(seconds=-1)


def _tools_path() -> Path:
    return CORE.data_dir / _TOOLS_SUBDIR


def _python_env_path(version: str) -> Path:
    return _tools_path() / "penvs" / version


def _framework_path(cache_key: str) -> Path:
    return _tools_path() / "frameworks" / cache_key


def _source_cache_key(ver_tag: str, source: ConfigType | None) -> str:
    """Distinguish a custom zephyr: sdk_source: from the official ver_tag-keyed cache dir,
    and from other sdk_source configs sharing the same version: value."""
    if source is None:
        return ver_tag
    if source[CONF_TYPE] == TYPE_LOCAL:
        path_hash = hashlib.sha1(str(source[CONF_PATH]).encode()).hexdigest()[:8]
        return f"{ver_tag}-local-{path_hash}"
    # TYPE_GIT
    ref = source.get(CONF_REF) or "HEAD"
    safe_ref = re.sub(r"[^A-Za-z0-9_.-]", "_", ref)
    url_hash = hashlib.sha1(source[CONF_URL].encode()).hexdigest()[:8]
    return f"{ver_tag}-{safe_ref}-{url_hash}"


def _requirements_path() -> Path:
    return Path(__file__).parent / "requirements_west.txt"


def _effective_requirements(overrides: dict[str, str | None]) -> str:
    """Return requirements_west.txt's content, substituting the version on any pinned
    `pkg==...` line whose package name has a non-None override (e.g. {"west": "1.4.0"},
    from CONF_WEST_VERSION/CONF_NINJA_VERSION)."""
    text = _requirements_path().read_text()
    if not any(overrides.values()):
        return text
    lines = []
    for line in text.splitlines():
        pkg = line.split("==", 1)[0]
        override = overrides.get(pkg)
        lines.append(f"{pkg}=={override}" if override else line)
    return "\n".join(lines) + "\n"


def check_and_install(
    sdk: ZephyrSDK,
    version: str,
    west_version: str | None = None,
    ninja_version: str | None = None,
    source: ConfigType | None = None,
    refresh: TimePeriodSeconds | None = None,
) -> tuple[Path, Path, dict[str, str]]:
    """Install west and Zephyr SDK.

    west_version/ninja_version override requirements_west.txt's pinned versions
    (CONF_WEST_VERSION/CONF_NINJA_VERSION). source, if given (zephyr: sdk_source:), overrides
    which Zephyr gets fetched -- a git fork/branch/ref (cloned into ESPHome's own managed
    cache, same as the official source), or a `local:` path to a manifest repo (e.g. a Zephyr
    fork checked out for local development). `local:` deliberately does NOT copy or symlink
    the checkout into ESPHome's cache -- west resolves symlinks when computing a workspace's
    topdir, so a symlink doesn't actually keep writes out of the real directory, and a copy
    would silently go stale against the user's live edits. Instead `west init -l` / `west
    update` run directly against the given path's own parent directory, which becomes the
    workspace topdir; `modules/`, `bootloader/`, etc. land there as siblings of the checkout,
    not inside ESPHome's own `.esphome/` tree. See project_zephyr_local_sdk_source_docs_todo
    for the write-up this needs once real docs exist (recommend a dedicated directory per
    checkout, not reusing an existing unrelated working tree).

    refresh controls how often a git source's moving ref is re-checked; `local:` always
    re-runs `west update` (cheap/no-op if nothing changed, since the user's checkout can move
    between builds) and the official source never needs it (always pinned to an immutable
    tag).

    Returns (python_bin, framework_path, west_env) where:
      - python_bin      — Python executable inside the managed venv
      - framework_path  — west workspace root (contains zephyr/, modules/, …)
      - west_env        — environment dict with ZEPHYR_BASE and PATH set
    """
    ver_tag = f"v{version}" if not version.startswith("v") else version
    python_env = _python_env_path(ver_tag)
    python_bin = get_python_env_executable_path(python_env, "python")

    is_local = source is not None and source[CONF_TYPE] == TYPE_LOCAL
    if is_local:
        local_path = source[
            CONF_PATH
        ]  # already a resolved Path -- cv.directory's return type
        if not (local_path / "west.yml").is_file():
            raise EsphomeError(
                f"'{local_path}' does not look like a Zephyr manifest repo (no west.yml) -- "
                "sdk_source: local: expects a path to a Zephyr checkout, e.g. a fork you're "
                "developing on."
            )
        framework = local_path.parent
        zephyr_dir = local_path
    else:
        framework = _framework_path(_source_cache_key(ver_tag, source))
        zephyr_dir = framework / "zephyr"

    west_env = {
        **os.environ,
        "ZEPHYR_BASE": str(zephyr_dir),
        "PATH": f"{python_bin.parent}{os.pathsep}{os.environ.get('PATH', '')}",
    }

    venv_sentinel = python_env / ".ready"
    overrides = {"west": west_version, "ninja": ninja_version}
    effective_requirements = _effective_requirements(overrides)

    install_venv = not python_bin.exists()
    # Compares content, not mtime -- an override changes what should be installed without
    # touching requirements_west.txt itself, so mtime alone would miss it.
    requirements_changed = (
        not venv_sentinel.exists()
        or venv_sentinel.read_text() != effective_requirements
    )

    if install_venv:
        rmdir(python_env, msg=f"Clean up {ver_tag} Python environment")
        create_venv(python_env, msg=ver_tag)

    if install_venv or requirements_changed:
        _LOGGER.info("Installing Python packages: %s", effective_requirements.split())
        requirements_file = python_env / ".requirements.effective.txt"
        requirements_file.write_text(effective_requirements)
        cmd = [
            str(python_bin),
            "-m",
            "pip",
            "install",
            "--upgrade",
            "-r",
            str(requirements_file),
        ]
        if not run_command_ok(cmd):
            raise EsphomeError(f"Upgrade {ver_tag} Python environment packages failure")
        venv_sentinel.write_text(effective_requirements)

    manifest_url = (
        source[CONF_URL] if (source is not None and not is_local) else sdk.manifest_url
    )
    manifest_rev = (
        source.get(CONF_REF) if (source is not None and not is_local) else ver_tag
    )
    label = str(source[CONF_PATH]) if is_local else (manifest_rev or "default branch")

    sentinel = framework / ".ready"
    needs_init = install_venv or not (framework / ".west").is_dir()
    # A git source: (moving ref) or local: source (user-edited checkout) can change
    # between builds, so always re-run `west update` -- cheap/no-op if unchanged. The
    # official source is pinned to an immutable tag and never needs this.
    needs_refresh = (
        is_local
        or (
            not needs_init
            and source is not None
            and refresh not in (None, NEVER_REFRESH)
            and not CORE.skip_external_update
            and (
                not sentinel.is_file()  # e.g. a prior update was interrupted before completing
                or (time.time() - sentinel.stat().st_mtime) > refresh.total_seconds
            )
        )
    )

    if needs_init:
        if is_local:
            # install_venv alone (framework/.west already present) doesn't need a re-init --
            # `west init` refuses to run against an already-initialized workspace.
            if not (framework / ".west").is_dir():
                _LOGGER.info("Initializing Zephyr SDK workspace for %s ...", label)
                cmd = [str(python_bin), "-m", "west", "init", "-l", local_path.name]
                if not run_command_ok(cmd, cwd=str(framework)):
                    raise EsphomeError(
                        f"Can't initialize Zephyr SDK workspace ({label})"
                    )
        else:
            _LOGGER.info("Initializing Zephyr SDK %s (%s) ...", ver_tag, label)
            rmdir(framework, msg=f"Clean up {ver_tag} framework")
            cmd = [str(python_bin), "-m", "west", "init", "-m", manifest_url]
            if manifest_rev:
                cmd += ["--mr", manifest_rev]
            cmd.append(str(framework))
            if not run_command_ok(cmd):
                raise EsphomeError(f"Can't initialize Zephyr SDK {ver_tag} ({label})")

    if needs_init or needs_refresh:
        _LOGGER.info(
            "Updating Zephyr SDK %s (%s) (this may take a while) ...", ver_tag, label
        )
        cmd = [
            str(python_bin),
            "-m",
            "west",
            "update",
            "--narrow",
            "--fetch-opt=--depth=1",
        ]
        env = os.environ.copy()
        env.update(west_env)
        result = subprocess.run(cmd, env=env, cwd=str(framework), check=False)
        if result.returncode != 0:
            raise EsphomeError(f"Can't update Zephyr SDK {ver_tag} ({label})")

        if needs_init:
            zephyr_reqs = zephyr_dir / "scripts" / "requirements.txt"
            if zephyr_reqs.exists():
                _LOGGER.info("Installing Zephyr Python requirements ...")
                req_cmd = [
                    str(python_bin),
                    "-m",
                    "pip",
                    "install",
                    "-r",
                    str(zephyr_reqs),
                ]
                if not run_command_ok(req_cmd):
                    raise EsphomeError("Can't install Zephyr Python requirements")

        sentinel.touch()

    return python_bin, framework, west_env
