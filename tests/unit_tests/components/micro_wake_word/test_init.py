"""Tests for the micro_wake_word model source validation and downloads."""

import json
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.components import micro_wake_word as mww
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_MODEL,
    CONF_PATH,
    CONF_REF,
    CONF_TYPE,
    CONF_URL,
)


@pytest.fixture
def mock_download_content_many() -> MagicMock:
    """Patch the concurrent download helper so no network is involved."""
    with patch(
        "esphome.components.micro_wake_word.external_files.download_content_many"
    ) as m:
        yield m


def test_shorthand_model_name_resolves_without_network(
    mock_download_content_many: MagicMock,
) -> None:
    config = mww._validate_source_shorthand("okay_nabu")
    assert config[CONF_TYPE] == mww.TYPE_HTTP
    assert config[CONF_URL] == (
        "https://github.com/esphome/micro-wake-word-models/raw/main/models/v2/okay_nabu.json"
    )
    mock_download_content_many.assert_not_called()


def test_shorthand_git_with_ref_not_captured_as_model_name(
    setup_core: Path, tmp_path: Path
) -> None:
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    (repo_dir / "model.json").write_text("{}")
    with patch(
        "esphome.components.micro_wake_word.git.clone_or_update",
        return_value=(repo_dir, None),
    ):
        config = mww._validate_source_shorthand("github://user/repo/model.json@main")
    assert config[CONF_TYPE] == "git"
    assert config[CONF_URL] == "https://github.com/user/repo.git"
    assert config[CONF_FILE] == "model.json"
    assert config[CONF_REF] == "main"


def test_shorthand_local_path_not_captured_as_model_name(
    setup_core: Path, tmp_path: Path
) -> None:
    manifest = tmp_path / "model.json"
    manifest.write_text("{}")
    config = mww.MODEL_SOURCE_SCHEMA(str(manifest))
    assert config[CONF_TYPE] == "local"
    assert Path(config[CONF_PATH]) == manifest


@pytest.mark.parametrize(
    "value", ["some/path/file", "name@ref", "bad:name", "okay_nabu\n", "héllo"]
)
def test_model_name_rejects_non_identifiers(value: str) -> None:
    with pytest.raises(cv.Invalid):
        mww._validate_source_model_name(value)


def _http_model(name: str) -> dict:
    return {
        CONF_MODEL: {
            CONF_TYPE: mww.TYPE_HTTP,
            CONF_URL: f"https://example.com/models/{name}.json",
        }
    }


def _write_manifest(model_config: dict, contents: str) -> Path:
    path = mww._compute_local_file_path(model_config[CONF_MODEL])
    path.mkdir(parents=True, exist_ok=True)
    manifest = path / "manifest.json"
    manifest.write_text(contents)
    return path


def test_download_http_models_batches_manifests_then_models(
    setup_core: Path, mock_download_content_many: MagicMock
) -> None:
    names = ("okay_nabu", "hey_mycroft", "vad")
    models = {name: _http_model(name) for name in names}
    paths = {
        name: _write_manifest(models[name], json.dumps({"model": f"{name}.tflite"}))
        for name in names
    }
    config = {
        mww.CONF_MODELS: [
            models["okay_nabu"],
            models["hey_mycroft"],
            # non-http sources must be ignored
            {CONF_MODEL: {CONF_TYPE: "local", CONF_PATH: "x"}},
        ],
        mww.CONF_VAD: models["vad"],
    }

    assert mww._download_http_models(config) is config

    assert mock_download_content_many.call_count == 2
    manifest_items = list(mock_download_content_many.call_args_list[0].args[0])
    assert manifest_items == [
        (f"https://example.com/models/{name}.json", paths[name] / "manifest.json")
        for name in names
    ]
    model_items = list(mock_download_content_many.call_args_list[1].args[0])
    assert model_items == [
        (f"https://example.com/models/{name}.tflite", paths[name] / f"{name}.tflite")
        for name in names
    ]


def test_download_http_models_no_http_sources_skips_download(
    mock_download_content_many: MagicMock,
) -> None:
    config = {mww.CONF_MODELS: [{CONF_MODEL: {CONF_TYPE: "local", CONF_PATH: "x"}}]}
    assert mww._download_http_models(config) is config
    mock_download_content_many.assert_not_called()


@pytest.mark.parametrize(
    ("contents", "message"),
    [
        ("not json", "Invalid manifest file"),
        ("[1, 2]", "must contain a JSON object"),
        ("{}", "missing the 'model' key"),
    ],
)
def test_download_http_models_bad_manifest_raises(
    setup_core: Path,
    mock_download_content_many: MagicMock,
    contents: str,
    message: str,
) -> None:
    model = _http_model("okay_nabu")
    config = {mww.CONF_MODELS: [model]}
    _write_manifest(model, contents)

    with pytest.raises(cv.Invalid, match=message):
        mww._download_http_models(config)
    # manifests were still fetched in one batch; the model batch never ran
    assert mock_download_content_many.call_count == 1


def test_download_http_models_collects_all_manifest_errors(
    setup_core: Path, mock_download_content_many: MagicMock
) -> None:
    models = {name: _http_model(name) for name in ("one", "two")}
    config = {mww.CONF_MODELS: list(models.values())}
    _write_manifest(models["one"], "not json")
    _write_manifest(models["two"], "[1]")

    with pytest.raises(cv.MultipleInvalid) as excinfo:
        mww._download_http_models(config)
    assert len(excinfo.value.errors) == 2
