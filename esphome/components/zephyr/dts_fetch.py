import hashlib
import logging
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

import yaml

import esphome.config_validation as cv
from esphome.const import (
    CONF_PATH,
    CONF_REF,
    CONF_TYPE,
    CONF_URL,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    TYPE_GIT,
    TYPE_LOCAL,
)
from esphome.core import CORE, TimePeriodSeconds
from esphome.types import ConfigType

from .const import KEY_ZEPHYR
from .variants import ZephyrSDK

_LOGGER = logging.getLogger(__name__)

_DTS_CACHE = Path.home() / ".esphome" / "zephyr_dts_cache"
# Bump when _sparse_clone_dts()'s `sparse-checkout set` file list changes, so a cache
# from before the change (still matching on tag alone) is detected as stale and
# re-fetched instead of silently missing the newly-added paths forever.
_SPARSE_CHECKOUT_SCHEMA = "2"
_SDK_SOURCE_VERSION_CACHE = Path.home() / ".esphome" / "zephyr_sdk_source_version_cache"
_MANIFEST_REVISION_CACHE = Path.home() / ".esphome" / "zephyr_manifest_revision_cache"

# A fork-pinned boards revision (e.g. Silabs' zephyr-silabs west.yml) is a raw commit
# SHA, which `git clone --branch` can't resolve.
_COMMIT_SHA_RE = re.compile(r"^[0-9a-f]{40}$")

_DTS_SPARSE_PATHS = (
    # scripts/dts/python-devicetree/ is the bundled edtlib, needed because the PyPI
    # package rejects newer binding keys (e.g. 'examples:').
    "boards/",
    "dts/",
    "include/zephyr/",
    "scripts/dts/python-devicetree/",
    "snippets/",
    # west.yml (top-level file) deliberately NOT listed here -- cone-mode
    # sparse-checkout only accepts directory patterns and already includes top-level
    # files automatically (see test_sparse_clone_dts_sparse_checkout_never_lists_version_file).
)

# Verified per-entry against a real failing #include (item 44), not guessed --
# esp32/renesas/rp2040/nordic board DTS needs nothing extra. Add a family only once
# its own failure against modules/hal/<name> is confirmed the same way.
_HAL_MODULES_BY_FAMILY: dict[str, tuple[str, ...]] = {
    "stm32": ("hal_stm32",),
}


def _git_sparse_fetch(
    repo: str, ref: str, dest: Path, paths: tuple[str, ...] = _DTS_SPARSE_PATHS
) -> None:
    """Fetch `ref` (a branch/tag name or a raw commit SHA) from `repo` into `dest`,
    sparse-checked-out to `paths` (default _DTS_SPARSE_PATHS).

    A raw SHA is fetched and checked out explicitly, since `git clone --branch` can't
    resolve it.
    """
    if _COMMIT_SHA_RE.fullmatch(ref):
        subprocess.run(
            ["git", "init", str(dest)], check=True, capture_output=True, text=True
        )
        subprocess.run(
            ["git", "-C", str(dest), "remote", "add", "origin", repo],
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            [
                "git",
                "-C",
                str(dest),
                "fetch",
                "--depth=1",
                "--filter=blob:none",
                "origin",
                ref,
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            ["git", "-C", str(dest), "sparse-checkout", "init", "--cone"],
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            ["git", "-C", str(dest), "sparse-checkout", "set", *paths],
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            ["git", "-C", str(dest), "checkout", "FETCH_HEAD"],
            check=True,
            capture_output=True,
            text=True,
        )
        return

    subprocess.run(
        [
            "git",
            "clone",
            "--depth=1",
            "--filter=blob:none",
            "--sparse",
            "--branch",
            ref,
            repo,
            str(dest),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    subprocess.run(
        ["git", "-C", str(dest), "sparse-checkout", "set", *_DTS_SPARSE_PATHS],
        check=True,
        capture_output=True,
        text=True,
    )


def _framework_base_version() -> str:
    """Return the SDK version string without PlatformIO build suffix (e.g. '2.6.1-b' → '2.6.1')."""
    ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    return f"{ver.major}.{ver.minor}.{ver.patch}"


def _parse_zephyr_version_file(version_file: Path) -> str:
    """Parse a Zephyr VERSION file (VERSION_MAJOR/VERSION_MINOR/PATCHLEVEL = N lines) into
    a 'major.minor.patch' string."""
    values: dict[str, str] = {}
    for line in version_file.read_text(encoding="utf-8").splitlines():
        if "=" not in line:
            continue
        key, _, value = line.partition("=")
        values[key.strip()] = value.strip()
    try:
        return f"{values['VERSION_MAJOR']}.{values['VERSION_MINOR']}.{values['PATCHLEVEL']}"
    except KeyError as exc:
        raise cv.Invalid(
            f"'{version_file}' is missing {exc} -- not a valid Zephyr VERSION file"
        ) from exc


def _sdk_source_cache_key(url: str, ref: str | None) -> str:
    return hashlib.sha1(f"{url}@{ref or 'HEAD'}".encode()).hexdigest()[:16]


def resolve_sdk_source_version(source: ConfigType, refresh: TimePeriodSeconds) -> str:
    """Return the 'major.minor.patch' version of a zephyr: sdk_source:, read directly from
    its VERSION file rather than guessed -- several places elsewhere in the codebase (build
    output layout, sysbuild mode, DTS lookups) branch on the exact semver, so this can't just
    default to the variant's own baseline when a custom source is in play."""
    if source[CONF_TYPE] == TYPE_LOCAL:
        # Points at the zephyr manifest repo itself, not a workspace root -- matches
        # framework_west.py's local_path/"west.yml" check.
        version_file = source[CONF_PATH] / "VERSION"
        if not version_file.is_file():
            raise cv.Invalid(
                f"'{version_file}' not found -- sdk_source: local: expects a path to a "
                "Zephyr checkout (e.g. a fork you're developing on)."
            )
        return _parse_zephyr_version_file(version_file)

    url = source[CONF_URL]
    ref = source.get(CONF_REF)
    dest = _SDK_SOURCE_VERSION_CACHE / _sdk_source_cache_key(url, ref)
    version_file = dest / "VERSION"

    needs_fetch = not version_file.is_file()
    if (
        not needs_fetch
        and refresh is not None
        and not CORE.skip_external_update
        and (time.time() - version_file.stat().st_mtime) > refresh.total_seconds
    ):
        needs_fetch = True

    if needs_fetch:
        shutil.rmtree(dest, ignore_errors=True)
        dest.parent.mkdir(parents=True, exist_ok=True)
        _LOGGER.info(
            "[zephyr] Checking sdk_source version (%s@%s) ...",
            url,
            ref or "default branch",
        )
        cmd = ["git", "clone", "--depth=1", "--filter=blob:none", "--sparse"]
        if ref:
            cmd += ["--branch", ref]
        cmd += [url, str(dest)]
        try:
            subprocess.run(cmd, check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as exc:
            raise cv.Invalid(
                f"Can't check sdk_source version: {exc.stderr.strip()}"
            ) from exc
        except FileNotFoundError as exc:
            raise cv.Invalid("sdk_source: requires git to be installed") from exc
        if not version_file.is_file():
            raise cv.Invalid(
                f"'{url}@{ref or 'default branch'}' has no top-level VERSION file -- "
                "is this really a Zephyr repository?"
            )

    return _parse_zephyr_version_file(version_file)


def _native_dts_path(sdk: ZephyrSDK) -> Path | None:
    """Return the zephyr/ subdirectory in the native SDK install if it is present on disk.

    Both mainline Zephyr and NCS check out the tree that owns boards/ under a sibling
    directory literally named "zephyr" (NCS: <tools_subdir>/frameworks/v<ver>/zephyr,
    the bundled sdk-zephyr fork project -- confirmed against a real local NCS install),
    so this needs no per-sdk branching.
    """
    if sdk.tools_subdir is None:
        return None
    candidate = (
        CORE.data_dir
        / sdk.tools_subdir
        / "frameworks"
        / f"v{_framework_base_version()}"
        / "zephyr"
    )
    return candidate if candidate.is_dir() else None


def _resolve_boards_ref(sdk: ZephyrSDK, ver: str) -> str | None:
    """Return the git ref to check out sdk.boards_repo_url at, for a resolved version `ver`.

    Mainline-shaped SDKs tag boards_repo_url (== manifest_url when unset) directly at
    f"v{ver}". SDKs with resolve_boards_ref_via_manifest=True (NCS) instead fetch
    manifest_url's own west.yml at tag f"v{ver}" and read the boards_repo_url project's
    own `revision:` field -- the only correct way to know it; see
    ZephyrSDK.resolve_boards_ref_via_manifest for why a guessed format string can't work.
    Cached at ~/.esphome/zephyr_manifest_revision_cache/ (small -- just the resolved
    string), immutable per (manifest_url, ver) since ver always maps to the same
    immutable upstream tag.
    """
    if not sdk.resolve_boards_ref_via_manifest:
        return f"v{ver}"

    cache_key = hashlib.sha1(f"{sdk.manifest_url}@v{ver}".encode()).hexdigest()[:16]
    cache_file = _MANIFEST_REVISION_CACHE / cache_key
    if cache_file.is_file():
        return cache_file.read_text().strip() or None

    with tempfile.TemporaryDirectory() as tmp:
        try:
            subprocess.run(
                [
                    "git",
                    "clone",
                    "--depth=1",
                    "--filter=blob:none",
                    "--sparse",
                    "--branch",
                    f"v{ver}",
                    sdk.manifest_url,
                    tmp,
                ],
                check=True,
                capture_output=True,
                text=True,
            )
        except (subprocess.CalledProcessError, FileNotFoundError) as exc:
            _LOGGER.warning(
                "[zephyr] Could not fetch %s@v%s to resolve boards revision: %s",
                sdk.manifest_url,
                ver,
                exc,
            )
            return None

        west_yml = Path(tmp) / "west.yml"
        if not west_yml.is_file():
            _LOGGER.warning(
                "[zephyr] %s@v%s has no top-level west.yml", sdk.manifest_url, ver
            )
            return None
        try:
            manifest = yaml.safe_load(west_yml.read_text())
        except yaml.YAMLError as exc:
            _LOGGER.warning("[zephyr] Could not parse %s: %s", west_yml, exc)
            return None

    # A project's manifest `name:` need not match its actual repo name -- match by
    # repo-path (falling back to name when repo-path is omitted, per west's own
    # resolution rules) against boards_repo_url's own basename instead.
    boards_repo_name = (sdk.boards_repo_url or "").rstrip("/").rsplit("/", 1)[-1]
    revision = None
    for project in manifest.get("manifest", {}).get("projects", []):
        if project.get("repo-path", project.get("name")) == boards_repo_name:
            revision = project.get("revision")
            break

    if revision is None:
        _LOGGER.warning(
            "[zephyr] %s@v%s's west.yml has no project matching %s",
            sdk.manifest_url,
            ver,
            sdk.boards_repo_url,
        )

    _MANIFEST_REVISION_CACHE.mkdir(parents=True, exist_ok=True)
    cache_file.write_text(revision or "")
    return revision


def _sparse_clone_hal_modules(zephyr_dir: Path, family: str | None) -> None:
    """Fetch dts/ only from each HAL module `family` needs (_HAL_MODULES_BY_FAMILY),
    into zephyr_dir.parent / <its west.yml path> -- matches the layout
    dts_lookup.py's _get_dts_include_paths() expects. Best-effort, never raises.
    """
    module_names = _HAL_MODULES_BY_FAMILY.get(family or "")
    if not module_names:
        return

    west_yml = zephyr_dir / "west.yml"
    if not west_yml.is_file():
        _LOGGER.warning(
            "[zephyr] %s has no west.yml; can't resolve HAL module(s) %s",
            zephyr_dir,
            ", ".join(module_names),
        )
        return
    try:
        manifest = yaml.safe_load(west_yml.read_text())
    except yaml.YAMLError as exc:
        _LOGGER.warning("[zephyr] Could not parse %s: %s", west_yml, exc)
        return

    manifest_root = manifest.get("manifest", {}) if manifest else {}
    default_remote = manifest_root.get("defaults", {}).get("remote")
    remote_bases = {
        r["name"]: r["url-base"]
        for r in manifest_root.get("remotes", [])
        if "name" in r and "url-base" in r
    }
    projects = manifest_root.get("projects", [])

    for name in module_names:
        project = next((p for p in projects if p.get("name") == name), None)
        if project is None:
            _LOGGER.warning(
                "[zephyr] %s's west.yml has no project named %s", west_yml, name
            )
            continue

        url_base = remote_bases.get(project.get("remote", default_remote))
        revision = project.get("revision")
        path = project.get("path")
        if not url_base or not revision or not path:
            _LOGGER.warning(
                "[zephyr] %s's west.yml project %s is missing remote/revision/path "
                "-- can't fetch it",
                west_yml,
                name,
            )
            continue

        url = f"{url_base}/{project.get('repo-path', name)}"
        dest = zephyr_dir.parent / path

        ref_marker = dest / ".resolved_ref"
        if ref_marker.is_file() and ref_marker.read_text().strip() == revision:
            continue

        if dest.is_dir():
            shutil.rmtree(dest, ignore_errors=True)
        dest.mkdir(parents=True, exist_ok=True)
        try:
            _git_sparse_fetch(url, revision, dest, paths=("dts/",))
            ref_marker.write_text(revision)
        except subprocess.CalledProcessError as exc:
            _LOGGER.warning(
                "[zephyr] HAL module fetch for %s failed: %s", name, exc.stderr.strip()
            )
            shutil.rmtree(dest, ignore_errors=True)
        except FileNotFoundError:
            _LOGGER.warning(
                "[zephyr] HAL module fetch requires git; install git and retry"
            )
            shutil.rmtree(dest, ignore_errors=True)


def _sparse_clone_dts(
    variant: str, sdk_name: str, sdk: ZephyrSDK, family: str | None = None
) -> Path | None:
    """Sparse-clone boards/, dts/, and include/zephyr/dt-bindings/ from the sdk's board
    repo.

    The clone is cached at ~/.esphome/zephyr_dts_cache/<variant>/<sdk_name>/<sdk_ver>/
    and reused on subsequent compiles. The cache is keyed on the resolved boards
    revision (stored in a small marker file), not just directory presence, so a stale
    checkout from before a ref-resolution fix landed is detected and re-fetched
    automatically instead of silently reused forever.
    Returns None if git is unavailable, the tag/ref can't be resolved, or the clone
    fails.
    """
    repo = sdk.boards_repo_url or sdk.manifest_url
    ver = _framework_base_version()
    dest = _DTS_CACHE / variant / sdk_name / ver

    tag = _resolve_boards_ref(sdk, ver)
    if tag is None:
        _LOGGER.warning(
            "[zephyr] Could not determine boards revision for %s %s %s; skipping sparse clone",
            variant,
            sdk_name,
            ver,
        )
        return None

    ref_marker = dest / ".resolved_ref"
    marker_content = f"{tag}#{_SPARSE_CHECKOUT_SCHEMA}"
    zephyr_dir = dest / "zephyr" if (dest / "zephyr").is_dir() else dest
    if (
        (zephyr_dir / "boards").is_dir()
        and ref_marker.is_file()
        and ref_marker.read_text().strip() == marker_content
    ):
        # HAL modules have their own marker -- may not have run yet on an older cache.
        _sparse_clone_hal_modules(zephyr_dir, family)
        return zephyr_dir

    if dest.is_dir():
        shutil.rmtree(dest, ignore_errors=True)  # stale cache -- force a re-fetch

    _LOGGER.info(
        "[zephyr] Fetching board DTS files for %s %s (revision %s) — one-time download, cached afterward",
        variant,
        ver,
        tag,
    )
    dest.mkdir(parents=True, exist_ok=True)
    try:
        _git_sparse_fetch(repo, tag, dest)
        # sdk-zephyr repos nest the Zephyr tree one level down
        zephyr_dir = dest / "zephyr" if (dest / "zephyr").is_dir() else dest
        if (zephyr_dir / "boards").is_dir():
            ref_marker.write_text(marker_content)
            _sparse_clone_hal_modules(zephyr_dir, family)
            return zephyr_dir
        return None
    except subprocess.CalledProcessError as exc:
        _LOGGER.warning("[zephyr] DTS fetch failed: %s", exc.stderr.strip())
    except FileNotFoundError:
        _LOGGER.warning("[zephyr] DTS fetch requires git; install git and retry")

    shutil.rmtree(dest, ignore_errors=True)
    return None


def _sparse_clone_dts_from_source(
    source: ConfigType, refresh: TimePeriodSeconds, family: str | None = None
) -> Path | None:
    """Sparse-clone boards/, dts/, and include/zephyr/ from a git sdk_source:.

    Mirrors _sparse_clone_dts() but clones the user's fork/ref directly instead of an
    upstream tag -- a fork's VERSION file (e.g. a custom bump like "4.4.99") has no
    matching tag on the plain upstream repo, so DTS lookups must come from the same
    fork/ref the rest of the build uses. Cached at
    ~/.esphome/zephyr_dts_cache/<url-ref-hash>/, keyed the same way as
    resolve_sdk_source_version()'s cache. Unlike the upstream-tag path (an immutable
    tag never needs re-fetching), a fork/branch is a moving ref -- respects the same
    `refresh:` window as resolve_sdk_source_version()/framework_west.py's SDK install,
    so a new commit pushed to the fork doesn't silently sit stale here indefinitely
    while the rest of the build correctly picks it up.
    """
    url = source[CONF_URL]
    ref = source.get(CONF_REF)
    dest = _DTS_CACHE / _sdk_source_cache_key(url, ref)
    schema_marker = dest / ".sparse_schema"

    zephyr_dir = dest / "zephyr" if (dest / "zephyr").is_dir() else dest
    if (zephyr_dir / "boards").is_dir():
        # A cache from before _SPARSE_CHECKOUT_SCHEMA's current value (e.g. one
        # predating snippets/ being added to the sparse-checkout set below) is
        # missing paths this schema version expects -- refetch immediately,
        # independent of the refresh: window, the same way _sparse_clone_dts()'s
        # own marker check does for the upstream-tag path.
        schema_stale = (
            not schema_marker.is_file()
            or schema_marker.read_text().strip() != _SPARSE_CHECKOUT_SCHEMA
        )
        stale = schema_stale or (
            refresh is not None
            and not CORE.skip_external_update
            and (time.time() - dest.stat().st_mtime) > refresh.total_seconds
        )
        if not stale:
            _sparse_clone_hal_modules(zephyr_dir, family)
            return zephyr_dir  # cache hit
        shutil.rmtree(dest, ignore_errors=True)

    _LOGGER.info(
        "[zephyr] Fetching board DTS files for sdk_source (%s@%s) — one-time download, cached afterward",
        url,
        ref or "default branch",
    )
    dest.mkdir(parents=True, exist_ok=True)
    try:
        if ref:
            _git_sparse_fetch(url, ref, dest)
        else:
            subprocess.run(
                [
                    "git",
                    "clone",
                    "--depth=1",
                    "--filter=blob:none",
                    "--sparse",
                    url,
                    str(dest),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run(
                ["git", "-C", str(dest), "sparse-checkout", "set", *_DTS_SPARSE_PATHS],
                check=True,
                capture_output=True,
                text=True,
            )
        zephyr_dir = dest / "zephyr" if (dest / "zephyr").is_dir() else dest
        if (zephyr_dir / "boards").is_dir():
            schema_marker.write_text(_SPARSE_CHECKOUT_SCHEMA)
            _sparse_clone_hal_modules(zephyr_dir, family)
            return zephyr_dir
        return None
    except subprocess.CalledProcessError as exc:
        _LOGGER.warning("[zephyr] DTS fetch failed: %s", exc.stderr.strip())
    except FileNotFoundError:
        _LOGGER.warning("[zephyr] DTS fetch requires git; install git and retry")

    shutil.rmtree(dest, ignore_errors=True)
    return None


async def fetch_board_dts(
    variant: str,
    sdk_name: str,
    sdk: ZephyrSDK,
    sdk_source: ConfigType | None,
    refresh: TimePeriodSeconds,
    family: str | None = None,
) -> None:
    """Populate ZephyrData dts_base_path with the local Zephyr tree.

    `sdk`/`sdk_name` are the already-resolved `zephyr: framework: type:` selection
    (VARIANTS[variant]'s own default sdk, or one of its alt_sdks) -- not necessarily
    the variant's default. A local sdk_source (a fork/checkout on disk) is used
    directly -- there's no version tag to clone by, and the checkout already has
    boards/dts/include on disk. A git sdk_source (fork/branch/ref) is sparse-cloned
    directly from that same fork/ref, for the same reason. Otherwise tries the native
    SDK install (no network required if already installed), then falls back to a
    sparse git clone of the resolved boards revision (see _resolve_boards_ref()),
    cached under ~/.esphome/zephyr_dts_cache/. Safe to call multiple times in one run —
    no-op after the first successful resolve.
    """
    zd = CORE.data[KEY_ZEPHYR]
    if zd.get("dts_base_path") is not None:
        return

    if sdk_source is not None and sdk_source[CONF_TYPE] == TYPE_LOCAL:
        zd["dts_base_path"] = str(sdk_source[CONF_PATH])
        _LOGGER.debug(
            "[zephyr] DTS base path: %s (local sdk_source)", zd["dts_base_path"]
        )
        return

    if sdk_source is not None and sdk_source[CONF_TYPE] == TYPE_GIT:
        zephyr_dir = _sparse_clone_dts_from_source(sdk_source, refresh, family)
        if zephyr_dir is not None:
            zd["dts_base_path"] = str(zephyr_dir)
            _LOGGER.debug("[zephyr] DTS base path: %s (git sdk_source)", zephyr_dir)
        else:
            _LOGGER.warning(
                "[zephyr] Board DTS files unavailable for sdk_source %s@%s; "
                "board-specific hardcodes remain as fallback.",
                sdk_source[CONF_URL],
                sdk_source.get(CONF_REF) or "default branch",
            )
        return

    zephyr_dir = _native_dts_path(sdk) or _sparse_clone_dts(
        variant, sdk_name, sdk, family
    )
    if zephyr_dir is not None:
        zd["dts_base_path"] = str(zephyr_dir)
        _LOGGER.debug("[zephyr] DTS base path: %s", zephyr_dir)
    else:
        _LOGGER.warning(
            "[zephyr] Board DTS files unavailable for %s %s %s; "
            "board-specific hardcodes remain as fallback.",
            variant,
            sdk_name,
            _framework_base_version(),
        )
