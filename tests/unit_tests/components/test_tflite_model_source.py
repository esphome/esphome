"""Tests for the tflite_model_source helpers (model sources, reports, heuristics)."""

import json
from pathlib import Path

import pytest

from esphome.components.tflite_model_source import model_source
from esphome.components.tflite_model_source.model_metadata import datasize_to_bytes
from esphome.components.tflite_model_source.model_source import TYPE_HTTP
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_MODEL,
    CONF_PASSWORD,
    CONF_TYPE,
    CONF_URL,
    CONF_USERNAME,
    TYPE_GIT,
    TYPE_LOCAL,
)
from esphome.core import CORE


@pytest.fixture(autouse=True)
def _config_dir(tmp_path, monkeypatch):
    """Point CORE at a temporary config dir (cv.file_ resolves paths against it)."""
    monkeypatch.setattr(model_source.CORE, "config_path", tmp_path / "config.yaml")


def _fake_download(contents_by_name=None, default=b"MODEL"):
    """Build a download_content stand-in that writes the bytes it returns."""
    contents_by_name = contents_by_name or {}
    calls = []

    def fake_download(url, path, *args, **kwargs):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        content = contents_by_name.get(path.name, default)
        if callable(content):
            content = content(url)
        path.write_bytes(content)
        calls.append((url, path))
        return content

    fake_download.calls = calls
    return fake_download


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("512", 512),
        ("512B", 512),
        ("4KB", 4096),
        ("1.5MB", 1572864),
        ("64K", 65536),
        ("1M", 1048576),
        ("2G", 2147483648),
        (1024, 1024),
    ],
)
def test_datasize_to_bytes(value, expected):
    assert datasize_to_bytes(value) == expected


def test_datasize_to_bytes_rejects_garbage():
    with pytest.raises(cv.Invalid):
        datasize_to_bytes("many")


def test_github_shorthand_rejects_a_malformed_value():
    """A ValueError from the parser is reported as a config error, not a crash."""
    with pytest.raises(cv.Invalid, match="Invalid git model shorthand") as err:
        model_source.MODEL_SOURCE_SCHEMA("github://own er/repo/models/digit.tflite")

    assert "github://<owner>/<repo>/<path-to-model>.tflite" in str(err.value)


def test_github_shorthand_with_model_suffix_is_cloned(tmp_path, monkeypatch):
    repo_dir = tmp_path / "repo"
    (repo_dir / "models").mkdir(parents=True)
    (repo_dir / "models" / "digit.tflite").write_bytes(b"MODEL")
    calls = []

    def fake_clone_or_update(**kwargs):
        calls.append(kwargs)
        return repo_dir, None

    monkeypatch.setattr("esphome.git.clone_or_update", fake_clone_or_update)

    config = model_source.MODEL_SOURCE_SCHEMA(
        "github://nliaudat/esphome_ai_component/models/digit.tflite@main"
    )

    assert config[CONF_TYPE] == TYPE_GIT
    assert config[CONF_URL] == "https://github.com/nliaudat/esphome_ai_component.git"
    assert config[CONF_FILE] == "models/digit.tflite"
    assert len(calls) == 1
    call = dict(calls[0])
    assert str(call.pop("refresh")) == "1d"
    assert call == {
        "url": "https://github.com/nliaudat/esphome_ai_component.git",
        "ref": "main",
        "domain": "tflite_model_source",
        "username": None,
        "password": None,
    }


def test_git_source_forwards_credentials(tmp_path, monkeypatch):
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    (repo_dir / "model.tflite").write_bytes(b"MODEL")
    calls = []

    def fake_clone_or_update(**kwargs):
        calls.append(kwargs)
        return repo_dir, None

    monkeypatch.setattr("esphome.git.clone_or_update", fake_clone_or_update)

    model_source.MODEL_SOURCE_SCHEMA(
        {
            CONF_TYPE: TYPE_GIT,
            CONF_URL: "https://example.com/private-model-repo.git",
            CONF_FILE: "model.tflite",
            CONF_USERNAME: "git-user",
            CONF_PASSWORD: "git-token",
        }
    )

    assert calls[0]["username"] == "git-user"
    assert calls[0]["password"] == "git-token"
    assert str(calls[0]["refresh"]) == "1d"


def test_github_shorthand_reports_missing_file(tmp_path, monkeypatch):
    repo_dir = tmp_path / "empty-repo"
    repo_dir.mkdir()
    monkeypatch.setattr(
        "esphome.git.clone_or_update", lambda **kwargs: (repo_dir, None)
    )

    with pytest.raises(cv.Invalid, match="Could not resolve git model"):
        model_source.MODEL_SOURCE_SCHEMA(
            "github://nliaudat/missing-repo/models/digit.tflite"
        )


@pytest.mark.parametrize(
    ("shorthand", "git_url"),
    [
        (
            "gitlab://nliaudat/ai-components/models/digit.tflite@main",
            "https://gitlab.com/nliaudat/ai-components.git",
        ),
        (
            "codeberg://nliaudat/ai-components/models/digit.tflite@main",
            "https://codeberg.org/nliaudat/ai-components.git",
        ),
    ],
)
def test_git_shorthands_other_than_github_are_cloned(
    tmp_path, monkeypatch, shorthand, git_url
):
    """The git parser supports GitLab and Codeberg, so the shorthand must too."""
    repo_dir = tmp_path / "repo"
    (repo_dir / "models").mkdir(parents=True)
    (repo_dir / "models" / "digit.tflite").write_bytes(b"MODEL")
    calls = []

    def fake_clone_or_update(**kwargs):
        calls.append(kwargs)
        return repo_dir, None

    monkeypatch.setattr("esphome.git.clone_or_update", fake_clone_or_update)

    config = model_source.MODEL_SOURCE_SCHEMA(shorthand)

    assert config[CONF_TYPE] == TYPE_GIT
    assert config[CONF_URL] == git_url
    assert config[CONF_FILE] == "models/digit.tflite"
    assert len(calls) == 1


def test_http_url_with_query_string_is_treated_as_model(tmp_path, monkeypatch):
    monkeypatch.setattr(model_source, "_cache_dir", lambda url: tmp_path)
    download = _fake_download()
    monkeypatch.setattr("esphome.external_files.download_content", download)

    config = model_source.MODEL_SOURCE_SCHEMA(
        "https://example.com/models/digit.tflite?raw=true"
    )

    assert config[CONF_TYPE] == TYPE_HTTP
    assert [(url, path.name) for url, path in download.calls] == [
        ("https://example.com/models/digit.tflite?raw=true", "digit.tflite"),
    ]


def test_http_manifest_with_invalid_json_is_reported(tmp_path, monkeypatch):
    monkeypatch.setattr(model_source, "_cache_dir", lambda url: tmp_path)
    monkeypatch.setattr(
        "esphome.external_files.download_content",
        _fake_download({"manifest.json": b"<html>not a manifest</html>"}),
    )

    with pytest.raises(cv.Invalid, match="neither a .tflite file nor a JSON manifest"):
        model_source.MODEL_SOURCE_SCHEMA("https://example.com/model")


def test_http_manifest_must_be_a_json_object(tmp_path, monkeypatch):
    monkeypatch.setattr(model_source, "_cache_dir", lambda url: tmp_path)
    monkeypatch.setattr(
        "esphome.external_files.download_content",
        _fake_download({"manifest.json": b'["digit.tflite"]'}),
    )

    with pytest.raises(cv.Invalid, match="must be a JSON object"):
        model_source.MODEL_SOURCE_SCHEMA("https://example.com/model")


def test_http_manifest_downloads_referenced_model(tmp_path, monkeypatch):
    monkeypatch.setattr(model_source, "_cache_dir", lambda url: tmp_path)
    download = _fake_download(
        {"manifest.json": json.dumps({"model": "digit.tflite"}).encode()}
    )
    monkeypatch.setattr("esphome.external_files.download_content", download)

    config = model_source.MODEL_SOURCE_SCHEMA(
        "https://example.com/manifests/digit.json"
    )

    assert config[CONF_TYPE] == TYPE_HTTP
    assert (tmp_path / "digit.tflite").read_bytes() == b"MODEL"
    assert download.calls[1][0] == "https://example.com/manifests/digit.tflite"


def test_local_model_source_round_trip(tmp_path):
    (tmp_path / "model.tflite").write_bytes(b"\x18\x00\x00\x00")

    config = model_source.MODEL_SOURCE_SCHEMA("model.tflite")
    path, data = model_source.resolve_model_source({CONF_MODEL: config})

    assert config[CONF_TYPE] == TYPE_LOCAL
    assert Path(path).name == "model.tflite"
    assert data == b"\x18\x00\x00\x00"


def test_resolve_model_source_reuses_validation_result(tmp_path, monkeypatch):
    repo_dir = tmp_path / "repo"
    (repo_dir / "models").mkdir(parents=True)
    (repo_dir / "models" / "digit.tflite").write_bytes(b"MODEL-BYTES")
    calls = []

    def fake_clone_or_update(**kwargs):
        calls.append(kwargs)
        return repo_dir, None

    monkeypatch.setattr("esphome.git.clone_or_update", fake_clone_or_update)

    config = model_source.MODEL_SOURCE_SCHEMA(
        "github://nliaudat/ai-components/models/digit.tflite"
    )
    path, data = model_source.resolve_model_source({CONF_MODEL: config})

    assert len(calls) == 1
    assert Path(path).name == "digit.tflite"
    assert data == b"MODEL-BYTES"


def test_resolve_model_source_rejects_unknown_type():
    with pytest.raises(cv.Invalid, match="Unknown model source type"):
        model_source.resolve_model_source({CONF_MODEL: {CONF_TYPE: "ftp"}})


def test_model_source_rejects_non_string_non_mapping():
    with pytest.raises(cv.Invalid, match="string or a mapping"):
        model_source.MODEL_SOURCE_SCHEMA(42)


def test_unresolvable_shorthand_reports_the_file_error():
    with pytest.raises(cv.Invalid, match="Could not find file") as err:
        model_source.MODEL_SOURCE_SCHEMA("models/digt.tflite")

    message = str(err.value)
    assert "Cannot use 'models/digt.tflite' as a local model file" in message
    assert "github://<owner>/<repo>/<path> shorthand" in message


def test_resolved_model_cache_is_dropped_by_core_reset(tmp_path, monkeypatch):
    """Resolved paths must not outlive the configuration run that found them."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    (repo_dir / "model.tflite").write_bytes(b"MODEL")
    calls = []

    def fake_clone_or_update(**kwargs):
        calls.append(kwargs)
        return repo_dir, None

    monkeypatch.setattr("esphome.git.clone_or_update", fake_clone_or_update)
    config = {
        CONF_MODEL: {
            CONF_TYPE: TYPE_GIT,
            CONF_URL: "https://example.com/model-repo.git",
            CONF_FILE: "model.tflite",
        }
    }

    assert model_source.resolve_model_source(config)[1] == b"MODEL"
    assert len(calls) == 1

    CORE.reset()
    assert model_source.resolve_model_source(config)[1] == b"MODEL"

    assert len(calls) == 2
