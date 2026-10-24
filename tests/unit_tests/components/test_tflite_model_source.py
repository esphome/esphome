"""Tests for the tflite_model_source helpers (model sources, reports, heuristics)."""

import json
import logging
from pathlib import Path
from types import SimpleNamespace

import pytest

from esphome.components.tflite_model_source import model_source
from esphome.components.tflite_model_source.model_metadata import (
    datasize_to_bytes,
    infer_model_config_from_filename,
    parse_audio_txt_file,
    parse_model_txt_file,
)
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


IMAGE_REPORT = (
    "Input 0: [  1  20  32   3]  numpy.uint8\n"
    "Output 0: [  1  10]\n"
    "Recommended tensor_arena_size: 64KB\n"
    "Total operations: 12\n"
)

AUDIO_REPORT = (
    "Input 0: [  1  16  96]  numpy.int8\n"
    "Recommended tensor_arena_size: 128KB\n"
    "probability_cutoff: 0.75\n"
    "features_step_size: 8\n"
)


def _write_report(tmp_path, content, name="digit"):
    (tmp_path / f"{name}.txt").write_text(content, encoding="utf-8")
    return tmp_path / f"{name}.tflite"


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


def _http_status_error(status_code: int) -> Exception:
    """Stand-in for the requests error behind a failed download.

    Only ``response.status_code`` is read, so the test does not depend on how
    requests builds its exceptions.
    """
    cause = Exception(f"{status_code} Client Error")
    cause.response = SimpleNamespace(status_code=status_code)
    return cause


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("512", 512),
        ("512B", 512),
        ("4KB", 4096),
        ("1.5MB", 1572864),
        (1024, 1024),
    ],
)
def test_datasize_to_bytes(value, expected):
    assert datasize_to_bytes(value) == expected


def test_datasize_to_bytes_rejects_garbage():
    with pytest.raises(cv.Invalid):
        datasize_to_bytes("many")


@pytest.mark.parametrize(
    ("filename", "channels", "order", "scale"),
    [
        ("mnist_RGB_10cls_32x20.tflite", 3, "RGB", 1.0),
        ("mnist_BGR_100cls.tflite", 3, "BGR", 10.0),
        ("mnist_GRAY_10cls.tflite", 1, "GRAY", 1.0),
        ("model.tflite", 3, "RGB", 1.0),
    ],
)
def test_infer_model_config_from_filename(filename, channels, order, scale):
    config = infer_model_config_from_filename(filename)

    assert config["input_channels"] == channels
    assert config["input_order"] == order
    assert config["scale_factor"] == scale


def test_parse_model_txt_file_without_report(tmp_path):
    assert parse_model_txt_file(tmp_path / "model.tflite") is None


def test_parse_model_txt_file_image_report(tmp_path):
    model_path = _write_report(tmp_path, IMAGE_REPORT)

    config = parse_model_txt_file(model_path)

    assert config["input_height"] == 20
    assert config["input_width"] == 32
    assert config["input_channels"] == 3
    assert config["input_type"] == "uint8"
    assert config["scale_factor"] == 1.0
    assert config["tensor_arena_size"] == 65536
    assert config["max_operators"] == 17
    assert config["output_processing"] == "softmax"


def test_parse_model_txt_file_with_softmax_reports_argmax(tmp_path):
    model_path = _write_report(tmp_path, IMAGE_REPORT + "  SOFTMAX: 1\n")

    assert parse_model_txt_file(model_path)["output_processing"] == "argmax"


def test_parse_model_txt_file_without_input_section_returns_none(tmp_path):
    model_path = _write_report(
        tmp_path, "Recommended tensor_arena_size: 64KB\nTotal operations: 12\n"
    )

    assert parse_model_txt_file(model_path) is None


@pytest.mark.parametrize("dtype", ["int8", "int16", "bfloat16"])
def test_parse_model_txt_file_rejects_unsupported_input_type(tmp_path, dtype):
    model_path = _write_report(
        tmp_path, IMAGE_REPORT.replace("numpy.uint8", f"numpy.{dtype}")
    )

    with pytest.raises(cv.Invalid, match="unsupported input type"):
        parse_model_txt_file(model_path)


@pytest.mark.parametrize(
    ("reported", "expected"),
    [
        ("64KB", 65536),
        ("0.5MB", 524288),
        ("131072B", 131072),
        ("64 KB", 65536),
    ],
)
def test_parse_model_txt_file_arena_units(tmp_path, reported, expected):
    model_path = _write_report(
        tmp_path,
        IMAGE_REPORT.replace("64KB", reported),
    )

    assert parse_model_txt_file(model_path)["tensor_arena_size"] == expected


def test_parse_model_txt_file_rejects_hybrid_quantization(tmp_path):
    model_path = _write_report(
        tmp_path,
        IMAGE_REPORT.replace("numpy.uint8", "numpy.float32") + "<class 'numpy.int8'>\n",
    )

    with pytest.raises(cv.Invalid, match="hybrid quantization"):
        parse_model_txt_file(model_path)


def test_parse_audio_txt_file(tmp_path):
    model_path = _write_report(tmp_path, AUDIO_REPORT)

    config = parse_audio_txt_file(model_path)

    assert config["sliding_window_size"] == 16
    assert config["feature_count"] == 96
    assert config["tensor_arena_size"] == 131072
    assert config["probability_cutoff"] == pytest.approx(0.75)
    assert config["features_step_size"] == 8


def test_parse_audio_txt_file_arena_in_megabytes(tmp_path):
    model_path = _write_report(tmp_path, AUDIO_REPORT.replace("128KB", "0.25MB"))

    assert parse_audio_txt_file(model_path)["tensor_arena_size"] == 262144


def test_parse_audio_txt_file_without_input_section_returns_none(tmp_path):
    model_path = _write_report(tmp_path, "features_step_size: 8\n")

    assert parse_audio_txt_file(model_path) is None


@pytest.mark.parametrize("dtype", ["int16", "bool"])
def test_parse_audio_txt_file_rejects_unsupported_input_type(tmp_path, dtype):
    """The feature buffer is filled in one dtype only, so anything else is an error."""
    model_path = _write_report(
        tmp_path, AUDIO_REPORT.replace("numpy.int8", f"numpy.{dtype}")
    )

    with pytest.raises(cv.Invalid, match="unsupported audio input type"):
        parse_audio_txt_file(model_path)


def test_parse_audio_txt_file_rejects_hybrid_quantization(tmp_path):
    model_path = _write_report(
        tmp_path,
        AUDIO_REPORT.replace("numpy.int8", "numpy.float32")
        + "Weights: <class 'numpy.int8'>\n",
    )

    with pytest.raises(cv.Invalid, match="hybrid quantization"):
        parse_audio_txt_file(model_path)


def test_github_shorthand_rejects_a_malformed_value():
    """A ValueError from the parser is reported as a config error, not a crash."""
    with pytest.raises(cv.Invalid, match="Invalid github:// model shorthand") as err:
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

    with pytest.raises(cv.Invalid, match="Could not resolve github:// model"):
        model_source.MODEL_SOURCE_SCHEMA(
            "github://nliaudat/missing-repo/models/digit.tflite"
        )


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
        ("https://example.com/models/digit.txt?raw=true", "digit.txt"),
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


def test_missing_companion_report_is_logged_at_info(tmp_path, monkeypatch, caplog):
    """A 404 for the optional .txt report is expected, so it is not a warning."""
    monkeypatch.setattr(model_source, "_cache_dir", lambda url: tmp_path)
    download = _fake_download()

    def fake_download(url, path, *args, **kwargs):
        if url.endswith(".txt"):
            raise cv.Invalid(
                f"Could not download from {url}: 404 Client Error"
            ) from _http_status_error(404)
        return download(url, path, *args, **kwargs)

    monkeypatch.setattr("esphome.external_files.download_content", fake_download)

    with caplog.at_level(logging.INFO, logger=model_source.__name__):
        model_source.MODEL_SOURCE_SCHEMA("https://example.com/models/digit.tflite")

    report_logs = [r for r in caplog.records if "digit.txt" in r.getMessage()]
    assert report_logs
    assert all(record.levelno == logging.INFO for record in report_logs)


def test_failed_companion_report_download_warns(tmp_path, monkeypatch, caplog):
    """A network or disk failure drops the metadata, so it deserves a warning."""
    monkeypatch.setattr(model_source, "_cache_dir", lambda url: tmp_path)
    download = _fake_download()

    def fake_download(url, path, *args, **kwargs):
        if url.endswith(".txt"):
            raise OSError("network is unreachable")
        return download(url, path, *args, **kwargs)

    monkeypatch.setattr("esphome.external_files.download_content", fake_download)

    with caplog.at_level(logging.INFO, logger=model_source.__name__):
        model_source.MODEL_SOURCE_SCHEMA("https://example.com/models/digit.tflite")

    report_logs = [r for r in caplog.records if "digit.txt" in r.getMessage()]
    assert report_logs
    assert all(record.levelno == logging.WARNING for record in report_logs)


def test_failed_companion_report_drops_the_cached_copy(tmp_path, monkeypatch, caplog):
    """A report left by an earlier run must not configure this run's model."""
    monkeypatch.setattr(model_source, "_cache_dir", lambda url: tmp_path)
    stale = tmp_path / "digit.txt"
    stale.write_text("Input 0: [  1  28  28   1]  numpy.uint8\n", encoding="utf-8")
    download = _fake_download()
    report_calls = []

    def fake_download(url, path, *args, **kwargs):
        if url.endswith(".txt"):
            report_calls.append(kwargs)
            raise OSError("network is unreachable")
        return download(url, path, *args, **kwargs)

    monkeypatch.setattr("esphome.external_files.download_content", fake_download)

    with caplog.at_level(logging.WARNING, logger=model_source.__name__):
        model_source.MODEL_SOURCE_SCHEMA("https://example.com/models/digit.tflite")

    assert report_calls == [{"allow_stale": False}]
    assert not stale.exists()


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
