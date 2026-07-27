import hashlib
import logging
from pathlib import Path
import shutil
import subprocess
import time

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
from .variants import VARIANTS

_LOGGER = logging.getLogger(__name__)

_DTS_CACHE = Path.home() / ".esphome" / "zephyr_dts_cache"
_SDK_SOURCE_VERSION_CACHE = Path.home() / ".esphome" / "zephyr_sdk_source_version_cache"


def _framework_base_version() -> str:
    """Return the SDK version string without PlatformIO build suffix (e.g. '2.6.1-b' → '2.6.1')."""
    ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    return f"{ver.major}.{ver.minor}.{ver.patch}"


def _parse_zephyr_version_file(version_file: Path) -> str:
    """Parse a Zephyr VERSION file (VERSION_MAJOR/VERSION_MINOR/PATCHLEVEL = N lines) into
    a 'major.minor.patch' string."""
    values: dict[str, str] = {}
    for line in version_file.read_text().splitlines():
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


def _native_dts_path(variant: str) -> Path | None:
    """Return the zephyr/ subdirectory in the native SDK install if it is present on disk."""
    variant_info = VARIANTS.get(variant)
    if variant_info is None or variant_info.sdk.tools_subdir is None:
        return None
    candidate = (
        CORE.data_dir
        / variant_info.sdk.tools_subdir
        / "frameworks"
        / f"v{_framework_base_version()}"
        / "zephyr"
    )
    return candidate if candidate.is_dir() else None


def _boards_clone_tag(variant: str, ver: str) -> str | None:
    """Return the git branch/tag to use when cloning board DTS files.

    For mainline Zephyr variants the version IS the tag (e.g. v4.4.0). A future
    variant on a vendor SDK with its own versioning scheme (distinct from the
    upstream sdk-zephyr tags it's built on) would need its user-visible version
    mapped to the underlying tag here, branching on `variant`.
    """
    return f"v{ver}"


def _sparse_clone_dts(variant: str) -> Path | None:
    """Sparse-clone boards/, dts/, and include/zephyr/dt-bindings/ from the variant SDK repo.

    The clone is cached at ~/.esphome/zephyr_dts_cache/<variant>/<sdk_ver>/ and reused on
    subsequent compiles.  Returns None if git is unavailable or the clone fails.
    """
    variant_info = VARIANTS.get(variant)
    if variant_info is None:
        return None

    sdk = variant_info.sdk
    repo = sdk.boards_repo_url or sdk.manifest_url
    ver = _framework_base_version()
    dest = _DTS_CACHE / variant / ver

    zephyr_dir = dest / "zephyr" if (dest / "zephyr").is_dir() else dest
    if (zephyr_dir / "boards").is_dir():
        return zephyr_dir  # cache hit

    tag = _boards_clone_tag(variant, ver)
    if tag is None:
        _LOGGER.warning(
            "[zephyr] Could not determine sdk-zephyr revision for %s %s; skipping sparse clone",
            variant,
            ver,
        )
        return None

    _LOGGER.info(
        "[zephyr] Fetching board DTS files for %s %s (revision %s) — one-time download, cached afterward",
        variant,
        ver,
        tag,
    )
    dest.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(
            [
                "git",
                "clone",
                "--depth=1",
                "--filter=blob:none",
                "--sparse",
                "--branch",
                tag,
                repo,
                str(dest),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            [
                "git",
                "-C",
                str(dest),
                "sparse-checkout",
                "set",
                # Cone-mode sparse-checkout only accepts directory patterns; VERSION is a
                # top-level file already included automatically, so it's not listed here.
                # Full include/zephyr/ is needed since dt-bindings headers transitively
                # include other core headers. scripts/dts/python-devicetree/ is the
                # bundled edtlib preferred over the PyPI package, which rejects newer
                # binding keys (e.g. 'examples:').
                "boards/",
                "dts/",
                "include/zephyr/",
                "scripts/dts/python-devicetree/",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        # sdk-zephyr repos nest the Zephyr tree one level down
        zephyr_dir = dest / "zephyr" if (dest / "zephyr").is_dir() else dest
        return zephyr_dir if (zephyr_dir / "boards").is_dir() else None
    except subprocess.CalledProcessError as exc:
        _LOGGER.warning("[zephyr] DTS fetch failed: %s", exc.stderr.strip())
    except FileNotFoundError:
        _LOGGER.warning("[zephyr] DTS fetch requires git; install git and retry")

    shutil.rmtree(dest, ignore_errors=True)
    return None


def _sparse_clone_dts_from_source(
    source: ConfigType, refresh: TimePeriodSeconds
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

    zephyr_dir = dest / "zephyr" if (dest / "zephyr").is_dir() else dest
    if (zephyr_dir / "boards").is_dir():
        stale = (
            refresh is not None
            and not CORE.skip_external_update
            and (time.time() - dest.stat().st_mtime) > refresh.total_seconds
        )
        if not stale:
            return zephyr_dir  # cache hit
        shutil.rmtree(dest, ignore_errors=True)

    _LOGGER.info(
        "[zephyr] Fetching board DTS files for sdk_source (%s@%s) — one-time download, cached afterward",
        url,
        ref or "default branch",
    )
    dest.mkdir(parents=True, exist_ok=True)
    try:
        cmd = ["git", "clone", "--depth=1", "--filter=blob:none", "--sparse"]
        if ref:
            cmd += ["--branch", ref]
        cmd += [url, str(dest)]
        subprocess.run(cmd, check=True, capture_output=True, text=True)
        subprocess.run(
            [
                "git",
                "-C",
                str(dest),
                "sparse-checkout",
                "set",
                "boards/",
                "dts/",
                "include/zephyr/",
                "scripts/dts/python-devicetree/",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        zephyr_dir = dest / "zephyr" if (dest / "zephyr").is_dir() else dest
        return zephyr_dir if (zephyr_dir / "boards").is_dir() else None
    except subprocess.CalledProcessError as exc:
        _LOGGER.warning("[zephyr] DTS fetch failed: %s", exc.stderr.strip())
    except FileNotFoundError:
        _LOGGER.warning("[zephyr] DTS fetch requires git; install git and retry")

    shutil.rmtree(dest, ignore_errors=True)
    return None


async def fetch_board_dts(
    variant: str, sdk_source: ConfigType | None, refresh: TimePeriodSeconds
) -> None:
    """Populate ZephyrData dts_base_path with the local Zephyr tree.

    A local sdk_source (a fork/checkout on disk) is used directly -- there's no
    version tag to clone by, and the checkout already has boards/dts/include on
    disk. A git sdk_source (fork/branch/ref) is sparse-cloned directly from that
    same fork/ref, for the same reason. Otherwise tries the native SDK install (no
    network required if already installed), then falls back to a sparse git clone
    of the upstream tag matching the resolved version, cached under
    ~/.esphome/zephyr_dts_cache/. Safe to call multiple times in one run —
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
        zephyr_dir = _sparse_clone_dts_from_source(sdk_source, refresh)
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

    zephyr_dir = _native_dts_path(variant) or _sparse_clone_dts(variant)
    if zephyr_dir is not None:
        zd["dts_base_path"] = str(zephyr_dir)
        _LOGGER.debug("[zephyr] DTS base path: %s", zephyr_dir)
    else:
        _LOGGER.warning(
            "[zephyr] Board DTS files unavailable for %s %s; "
            "board-specific hardcodes remain as fallback.",
            variant,
            _framework_base_version(),
        )
