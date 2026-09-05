import hashlib
import logging
import os
from pathlib import Path
import re
import subprocess

import yaml

from esphome.build_helpers.tools_cache import SDK_ZEPHYR_TOOLS_CACHE, tools_cache_path
from esphome.const import CONF_PATH, CONF_REF, CONF_TYPE, CONF_URL, TYPE_LOCAL
from esphome.core import EsphomeError
from esphome.framework_helpers import (
    create_venv,
    get_python_env_executable_path,
    rmdir,
    run_command_ok,
)
from esphome.types import ConfigType

from .const import KEY_SDK_SOURCE_RESOLVED_REF
from .variants import ZephyrModule, ZephyrSDK

_LOGGER = logging.getLogger(__name__)


def _tools_path() -> Path:
    # Machine-global (OS user cache dir) so all Zephyr-based projects share one
    # SDK/toolchain/west install; see build_helpers.tools_cache.tools_cache_path
    # for the env-override and normalization rules.
    return tools_cache_path(*SDK_ZEPHYR_TOOLS_CACHE)


def _python_env_path(version: str) -> Path:
    return _tools_path() / "penvs" / version


def _framework_path(cache_key: str) -> Path:
    return _tools_path() / "frameworks" / cache_key


def _source_cache_key(
    sdk: ZephyrSDK,
    ver_tag: str,
    source: ConfigType | None,
    git_modules: list[ZephyrModule] | None = None,
) -> str:
    """Distinguish a custom zephyr: sdk_source: from the official ver_tag-keyed cache dir,
    and from other sdk_source configs sharing the same version: value. A non-empty
    git_modules set also gets its own cache dir -- a different active module *set*
    changes what the generated manifest fetches, so it can't share a workspace with a
    no-module build or a different combination.

    sdk.tools_subdir prefixes the key so MAINLINE/NCS/SILABS -- which now share one
    machine-global cache root -- can't collide on a matching version: string (e.g. NCS's
    bundled Zephyr fork tracks close to mainline's own numbering)."""
    if source is None:
        key = f"{sdk.tools_subdir}-{ver_tag}"
    elif source[CONF_TYPE] == TYPE_LOCAL:
        path_hash = hashlib.sha1(str(source[CONF_PATH]).encode()).hexdigest()[:8]
        key = f"{sdk.tools_subdir}-{ver_tag}-local-{path_hash}"
    else:
        # TYPE_GIT
        ref = source.get(CONF_REF) or "HEAD"
        safe_ref = re.sub(r"[^A-Za-z0-9_.-]", "_", ref)
        url_hash = hashlib.sha1(source[CONF_URL].encode()).hexdigest()[:8]
        key = f"{sdk.tools_subdir}-{ver_tag}-{safe_ref}-{url_hash}"
    if not git_modules:
        return key
    modules_id = "|".join(
        f"{m.name}@{m.manifest_url}@{m.revision}"
        for m in sorted(git_modules, key=lambda m: m.name)
    )
    modules_hash = hashlib.sha1(modules_id.encode()).hexdigest()[:8]
    return f"{key}-mod-{modules_hash}"


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


def _generate_synthetic_manifest(
    framework: Path,
    manifest_url: str,
    manifest_rev: str | None,
    git_modules: list[ZephyrModule],
    west_project_name: str | None = None,
) -> Path:
    """Compose a local west manifest that imports the root SDK and every active
    git-sourced module as sibling projects, instead of rooting the workspace at the
    SDK's own repo directly. Needed once more than one thing has to be fetched
    together -- `west init -m` only supports a single top-level manifest repo.
    Verified against real sdk-nrf/ncs-zigbee manifests: both resolve the shared
    `zephyr` project to a single revision with no conflict, since it's only imported
    once (via whichever project lists it first).

    west_project_name overrides the root project's west name (see
    ZephyrSDK.west_project_name) -- required for NCS, whose own sysbuild/Kconfig
    scripts hardcode "nrf" as the project name and break if it's named anything else
    (e.g. the URL-basename default, "sdk-nrf").
    """
    manifest_dir = framework / "esphome-manifest"
    root_name = west_project_name or manifest_url.rstrip("/").rsplit("/", 1)[-1]
    root_project = {"name": root_name, "url": manifest_url, "import": True}
    # A ref-less `sdk_source: {type: git}` means "track the source's default branch"
    # -- omitting revision: here matches that, same as the plain `west init -m`
    # path's `if manifest_rev: cmd += ["--mr", manifest_rev]`. Writing `revision:
    # null` instead would hand west an explicit null the manifest schema doesn't
    # expect.
    if manifest_rev:
        root_project["revision"] = manifest_rev
    projects = [root_project]
    projects.extend(
        {
            "name": module.name,
            "url": module.manifest_url,
            "revision": module.revision,
            "import": True,
        }
        for module in git_modules
    )
    manifest_yaml = yaml.safe_dump(
        {"manifest": {"projects": projects, "self": {"path": "esphome-manifest"}}},
        sort_keys=False,
    )
    manifest_dir.mkdir(parents=True, exist_ok=True)
    (manifest_dir / "west.yml").write_text(manifest_yaml)

    # `west init -l` treats the given directory as a manifest repository -- its own
    # git working tree, not just a plain directory of files.
    if not (manifest_dir / ".git").is_dir() and not run_command_ok(
        ["git", "init", "-q"], cwd=str(manifest_dir)
    ):
        raise EsphomeError("Can't initialize the generated Zephyr manifest repository")
    if not run_command_ok(["git", "add", "west.yml"], cwd=str(manifest_dir)):
        raise EsphomeError("Can't stage the generated Zephyr manifest")
    # -c user.*: this commit is purely internal (never pushed, never read by a human),
    # so it shouldn't depend on -- or touch -- the user's own global git identity.
    # --allow-empty: a re-generation with unchanged content is a no-op commit, not an
    # error.
    run_command_ok(
        [
            "git",
            "-c",
            "user.name=ESPHome",
            "-c",
            "user.email=esphome@esphome.io",
            "commit",
            "-q",
            "--allow-empty",
            "-m",
            "esphome-generated Zephyr manifest",
        ],
        cwd=str(manifest_dir),
    )
    return manifest_dir


def check_and_install(
    sdk: ZephyrSDK,
    version: str,
    west_version: str | None = None,
    ninja_version: str | None = None,
    source: ConfigType | None = None,
    modules: list[ZephyrModule] | None = None,
) -> tuple[Path, Path, dict[str, str]]:
    """Install west and Zephyr SDK.

    modules (resolve_zephyr_modules()'s result): additive west modules to fetch
    alongside the SDK, each as its own sibling project in a generated manifest --
    see _generate_synthetic_manifest(). A module with local_path set (zephyr:
    modules: type: local) isn't a west project at all -- the caller wires it into the
    build directly via EXTRA_ZEPHYR_MODULES instead, same as a converted PlatformIO
    library.

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

    A git source is pinned to one resolved commit SHA before this function ever runs (see
    dts_fetch.resolve_sdk_source_version(), which stashes it onto source under
    KEY_SDK_SOURCE_RESOLVED_REF) -- like the official source, pinned to an immutable tag,
    it never needs an independent re-check here. `local:` is the one case that always
    re-runs `west update` (cheap/no-op if nothing changed, since the user's checkout can
    move between builds).

    Returns (python_bin, framework_path, west_env) where:
      - python_bin      — Python executable inside the managed venv
      - framework_path  — west workspace root (contains zephyr/, modules/, …)
      - west_env        — environment dict with ZEPHYR_BASE and PATH set
    """
    ver_tag = f"v{version}" if not version.startswith("v") else version
    python_env = _python_env_path(ver_tag)
    python_bin = get_python_env_executable_path(python_env, "python")

    git_modules = [m for m in (modules or []) if m.manifest_url is not None]

    is_local = source is not None and source[CONF_TYPE] == TYPE_LOCAL
    if is_local:
        if git_modules:
            raise EsphomeError(
                "zephyr: modules: can't be combined with sdk_source: local: -- "
                "local: points at your own checkout, which ESPHome can't safely "
                "graft additional projects onto."
            )
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
        framework = _framework_path(
            _source_cache_key(sdk, ver_tag, source, git_modules)
        )
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
        source.get(KEY_SDK_SOURCE_RESOLVED_REF, source.get(CONF_REF))
        if (source is not None and not is_local)
        else ver_tag
    )
    label = str(source[CONF_PATH]) if is_local else (manifest_rev or "default branch")

    sentinel = framework / ".ready"
    needs_init = install_venv or not (framework / ".west").is_dir()
    # local: (a user-edited checkout) can change between builds, so always re-run
    # `west update` -- cheap/no-op if unchanged. A git source: is pinned to an exact
    # resolved commit before this function runs (dts_fetch.resolve_sdk_source_version()),
    # so -- like the official source, pinned to an immutable tag -- it never needs this.
    needs_refresh = is_local

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
        elif git_modules:
            module_names = ", ".join(m.name for m in git_modules)
            _LOGGER.info(
                "Initializing Zephyr SDK %s with modules (%s, %s) ...",
                ver_tag,
                label,
                module_names,
            )
            rmdir(framework, msg=f"Clean up {ver_tag} framework")
            manifest_dir = _generate_synthetic_manifest(
                framework,
                manifest_url,
                manifest_rev,
                git_modules,
                west_project_name=sdk.west_project_name,
            )
            cmd = [str(python_bin), "-m", "west", "init", "-l", manifest_dir.name]
            if not run_command_ok(cmd, cwd=str(framework)):
                raise EsphomeError(
                    f"Can't initialize Zephyr SDK {ver_tag} with modules "
                    f"({module_names}) ({label})"
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
