"""Model source resolution and download for the tflite_model_source component.

Handles local file, github:// shorthand, and http(s):// URL model sources,
including external-file caching and JSON manifest downloads.
"""

from contextlib import suppress
import json
import logging
from pathlib import Path
import posixpath
from urllib.parse import urljoin, urlparse, urlunparse

import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_MODEL,
    CONF_PASSWORD,
    CONF_PATH,
    CONF_REF,
    CONF_REFRESH,
    CONF_TYPE,
    CONF_URL,
    CONF_USERNAME,
    TYPE_GIT,
    TYPE_LOCAL,
)
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

# Component name for external-file caching and git clones (see the DOMAIN
# attribute exported by the package __init__).
_DOMAIN = "tflite_model_source"

TYPE_HTTP = "http"

# Local file paths, github:// shorthands and http(s):// URLs that point at a
# model file share this suffix; every other remote source is a JSON manifest.
MODEL_SUFFIX = ".tflite"

# Resolved model files, keyed by source. Validation resolves (clones/downloads)
# once and code generation reuses the result instead of doing the work again.
_RESOLVED_PATHS: dict[tuple[str, ...], Path] = {}


def _cache_dir(url: str) -> Path:
    from esphome import external_files

    return external_files.compute_local_file_path(_DOMAIN, url)


def _is_model_url(url: str) -> bool:
    """Whether the URL path (ignoring any query string) points at a model file."""
    return urlparse(url).path.lower().endswith(MODEL_SUFFIX)


def _download_companion_txt(model_url: str, cache_dir: Path) -> None:
    """Fetch the optional ``.txt`` metadata report that sits next to the model."""
    from esphome import external_files

    parts = urlparse(model_url)
    txt_url = urlunparse(
        parts._replace(path=posixpath.splitext(parts.path)[0] + ".txt")
    )
    txt_path = cache_dir / posixpath.basename(urlparse(txt_url).path)
    # Missing report is fine: configuration falls back to filename heuristics.
    with suppress(cv.Invalid, OSError):
        external_files.download_content(txt_url, txt_path)


def _git_model_path(config) -> Path:
    """Clone (or update) the repository and return the model file inside it."""
    key = (TYPE_GIT, config[CONF_URL], config.get(CONF_REF, ""), config[CONF_FILE])
    if (path := _RESOLVED_PATHS.get(key)) is not None:
        _LOGGER.debug("Reusing resolved model file %s", path)
        return path

    from esphome import git

    repo_dir, _ = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=config.get(CONF_REFRESH),
        domain=_DOMAIN,
        username=config.get(CONF_USERNAME),
        password=config.get(CONF_PASSWORD),
    )
    model_path = repo_dir / config[CONF_FILE]
    if not model_path.is_file():
        raise cv.Invalid(f"File does not exist in repository: {config[CONF_FILE]}")

    _RESOLVED_PATHS[key] = model_path
    return model_path


def _http_model_path(url: str) -> Path:
    """Download the model (directly or from its JSON manifest) and return it."""
    key = (TYPE_HTTP, url)
    if (path := _RESOLVED_PATHS.get(key)) is not None:
        _LOGGER.debug("Reusing downloaded model file %s", path)
        return path

    from esphome import external_files

    cache_dir = _cache_dir(url)
    if _is_model_url(url):
        model_url = url
        model_path = cache_dir / posixpath.basename(urlparse(url).path)
    else:
        manifest_path = cache_dir / "manifest.json"
        manifest_contents = external_files.download_content(url, manifest_path)
        try:
            manifest = json.loads(manifest_contents)
        except ValueError as err:
            raise cv.Invalid(
                f"{url} is neither a {MODEL_SUFFIX} file nor a JSON manifest: {err}"
            ) from err
        if not isinstance(manifest, dict):
            raise cv.Invalid(f"Manifest at {url} must be a JSON object")
        manifest_model = manifest.get(CONF_MODEL)
        if not isinstance(manifest_model, str) or not manifest_model:
            raise cv.Invalid(f"Manifest at {url} does not specify a model file")
        model_url = urljoin(url, manifest_model)
        model_path = cache_dir / posixpath.basename(urlparse(model_url).path)

    external_files.download_content(model_url, model_path)
    _download_companion_txt(model_url, cache_dir)

    _RESOLVED_PATHS[key] = model_path
    return model_path


def _validate_github_shorthand(value: str):
    from esphome import git

    git_file = git.GitFile.from_shorthand(value)
    conf = {
        CONF_TYPE: TYPE_GIT,
        CONF_URL: git_file.git_url,
        CONF_FILE: git_file.filename,
    }
    if git_file.ref:
        conf[CONF_REF] = git_file.ref
    try:
        return MODEL_SOURCE_MAPPING_SCHEMA(conf)
    except cv.Invalid as err:
        raise cv.Invalid(f"Could not resolve github:// model '{value}': {err}") from err


def _validate_source_shorthand(value):
    if not isinstance(value, str):
        raise cv.Invalid("Model source must be a string or a mapping")

    # Scheme prefixes are checked before the .tflite suffix so that a
    # github://owner/repo/models/digit.tflite shorthand is cloned instead of
    # being treated as a local path.
    if value.startswith("github://"):
        return _validate_github_shorthand(value)
    if value.startswith(("http://", "https://")):
        return MODEL_SOURCE_MAPPING_SCHEMA({CONF_TYPE: TYPE_HTTP, CONF_URL: value})
    try:
        return MODEL_SOURCE_MAPPING_SCHEMA({CONF_TYPE: TYPE_LOCAL, CONF_PATH: value})
    except cv.Invalid as err:
        raise cv.Invalid(
            f"Cannot resolve model source '{value}': use a local {MODEL_SUFFIX} path, "
            "a github://<owner>/<repo>/<path> shorthand or an http(s):// URL"
        ) from err


def _process_git_source(config):
    _git_model_path(config)
    return config


GIT_SCHEMA = cv.All(
    cv.GIT_SCHEMA.extend(
        {
            cv.Required(CONF_FILE): cv.string,
            cv.Optional(CONF_REFRESH, default="1d"): cv.All(
                cv.string, cv.source_refresh
            ),
        }
    ),
    _process_git_source,
)


def _process_http_source(config):
    _http_model_path(config[CONF_URL])
    return config


HTTP_SCHEMA = cv.All({cv.Required(CONF_URL): cv.url}, _process_http_source)


LOCAL_SCHEMA = cv.Schema({cv.Required(CONF_PATH): cv.All(cv.file_, cv.string)})


MODEL_SOURCE_MAPPING_SCHEMA = cv.typed_schema(
    {
        TYPE_GIT: GIT_SCHEMA,
        TYPE_LOCAL: LOCAL_SCHEMA,
        TYPE_HTTP: HTTP_SCHEMA,
    }
)


def validate_model_source(value):
    """Validate a model source shorthand string or typed mapping."""
    if isinstance(value, dict):
        return MODEL_SOURCE_MAPPING_SCHEMA(value)
    return _validate_source_shorthand(value)


MODEL_SOURCE_SCHEMA = validate_model_source


def resolve_model_source(entry_config):
    """Return the resolved ``(path, bytes)`` of the configured model."""
    model_spec = entry_config[CONF_MODEL]
    source_type = model_spec[CONF_TYPE]
    if source_type == TYPE_LOCAL:
        return _read_model_file(CORE.relative_config_path(model_spec[CONF_PATH]))
    if source_type == TYPE_GIT:
        return _read_model_file(_git_model_path(model_spec))
    if source_type == TYPE_HTTP:
        return _read_model_file(_http_model_path(model_spec[CONF_URL]))
    raise cv.Invalid(f"Unknown model source type: {source_type}")


def _read_model_file(model_path) -> tuple[Path, bytes]:
    model_path = Path(model_path)
    try:
        model_data = model_path.read_bytes()
    except OSError as err:
        raise cv.Invalid(f"Could not read model file {model_path}: {err}") from err
    return model_path, model_data
