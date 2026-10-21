"""Model source resolution and download for the tflite_model_source component.

Handles local file, github:// shorthand, and http(s):// URL model sources,
including external-file caching and JSON manifest downloads.
"""

from contextlib import suppress
import hashlib
import json
from pathlib import Path
from urllib.parse import urljoin

import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_MODEL,
    CONF_PATH,
    CONF_REF,
    CONF_REFRESH,
    CONF_TYPE,
    CONF_URL,
    TYPE_GIT,
    TYPE_LOCAL,
)
from esphome.core import CORE

# Component name for external-file caching and git clones (see the DOMAIN
# attribute exported by the package __init__).
_DOMAIN = "tflite_model_source"

TYPE_HTTP = "http"


def _compute_local_file_path(url):
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    from esphome import external_files

    base_dir = external_files.compute_local_file_dir(_DOMAIN)
    return base_dir / key


def _validate_source_shorthand(value):
    if not isinstance(value, str):
        raise cv.Invalid("Model source must be a string or dict")
    if Path(value).exists():
        return MODEL_SOURCE_SCHEMA({CONF_TYPE: TYPE_LOCAL, CONF_PATH: value})
    if value.startswith(("http://", "https://")):
        return MODEL_SOURCE_SCHEMA({CONF_TYPE: TYPE_HTTP, CONF_URL: value})
    if value.endswith(".tflite"):
        return MODEL_SOURCE_SCHEMA({CONF_TYPE: TYPE_LOCAL, CONF_PATH: value})
    if value.startswith("github://"):
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
            return MODEL_SOURCE_SCHEMA(conf)
        except cv.Invalid as e:
            raise cv.Invalid(f"Could not resolve github:// model: {value}") from e
    raise cv.Invalid(
        f"Cannot resolve model source: '{value}'. "
        f"Use a local .tflite path, github:// URL, or http(s):// URL."
    )


CV_GIT_SCHEMA = cv.GIT_SCHEMA
if isinstance(CV_GIT_SCHEMA, dict):
    CV_GIT_SCHEMA = cv.Schema(CV_GIT_SCHEMA)


def _process_git_source(config):
    from esphome import git

    repo_dir, _ = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=config.get(CONF_REFRESH, "1d"),
        domain=_DOMAIN,
    )
    if not (repo_dir / config[CONF_FILE]).exists():
        raise cv.Invalid(f"File does not exist in repository: {config[CONF_FILE]}")
    return config


GIT_SCHEMA = cv.All(
    CV_GIT_SCHEMA.extend(
        {
            cv.Required(CONF_FILE): cv.string,
            cv.Optional(CONF_REFRESH, default="1d"): cv.All(
                cv.string, cv.source_refresh
            ),
        }
    ),
    _process_git_source,
)


def _download_http_model(url):
    """Download a model from an http(s) URL and return its local path.

    Supports both JSON manifests (which reference the model file) and direct
    ``.tflite`` URLs. For direct model URLs the companion ``.txt`` metadata
    file (same basename, ``.txt`` extension) is also fetched best-effort so
    image/audio model auto-config stays available.
    """
    from esphome import external_files

    path = _compute_local_file_path(url)
    if url.lower().endswith(".tflite"):
        model_path = path / Path(url).name
        external_files.download_content(url, model_path)
        # Optional companion .txt metadata file; missing is fine, config
        # auto-detection falls back to filename heuristics.
        txt_url = f"{url[:-7]}.txt"
        txt_path = path / Path(txt_url).name
        with suppress(cv.Invalid, OSError):
            external_files.download_content(txt_url, txt_path)
        return model_path

    manifest_path = path / "manifest.json"
    json_contents = external_files.download_content(url, manifest_path)
    manifest_data = json.loads(json_contents)
    model_file = manifest_data.get("model")
    if not model_file:
        raise cv.Invalid(f"Manifest at {url} does not specify a model file")
    model_url = urljoin(url, model_file)
    model_path = path / Path(model_file).name
    external_files.download_content(str(model_url), model_path)
    return model_path


def _process_http_source(config):
    _download_http_model(config[CONF_URL])
    return config


HTTP_SCHEMA = cv.All({cv.Required(CONF_URL): cv.url}, _process_http_source)


LOCAL_SCHEMA = cv.Schema({cv.Required(CONF_PATH): cv.All(cv.file_, cv.string)})


MODEL_SOURCE_SCHEMA = cv.Any(
    _validate_source_shorthand,
    cv.typed_schema(
        {
            TYPE_GIT: GIT_SCHEMA,
            TYPE_LOCAL: LOCAL_SCHEMA,
            TYPE_HTTP: HTTP_SCHEMA,
        }
    ),
    msg="Not a valid model path, github:// shorthand, or http(s):// URL",
)


def resolve_model_source(entry_config):
    model_spec = entry_config[CONF_MODEL]
    if isinstance(model_spec, str):
        return _load_local_file(model_spec)
    source_type = model_spec.get(CONF_TYPE, "local")
    if source_type == "local":
        return _load_local_file(model_spec[CONF_PATH])
    if source_type == "git":
        return _load_git_file(model_spec)
    if source_type == "http":
        return _load_http_file(model_spec)
    raise cv.Invalid(f"Unknown model source type: {source_type}")


def _load_local_file(path):
    model_path = CORE.relative_config_path(path)
    if not Path(model_path).exists():
        raise cv.Invalid(f"Model file not found: {model_path}")
    with Path(model_path).open("rb") as f:
        model_data = f.read()
    return model_path, model_data


def _load_git_file(config):
    from esphome import git

    repo_dir, _ = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=config.get(CONF_REFRESH, "1d"),
        domain=_DOMAIN,
    )
    model_path = repo_dir / config[CONF_FILE]
    if not model_path.exists():
        raise cv.Invalid(f"Model file not found in repository: {model_path}")
    with model_path.open("rb") as f:
        model_data = f.read()
    return model_path, model_data


def _load_http_file(config):
    model_path = _download_http_model(config[CONF_URL])
    with Path(model_path).open("rb") as f:
        model_data = f.read()
    return model_path, model_data
