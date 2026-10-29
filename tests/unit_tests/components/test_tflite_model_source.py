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


@pytest.fixture(autouse=True)
def _fresh_downloads(monkeypatch):
    """Treat the stubbed downloads as verified unless a test says otherwise."""
    monkeypatch.setattr("esphome.external_files.is_fresh_this_run", lambda path: True)


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


def test_infer_model_config_from_filename_warns_about_the_defaults(caplog):
    """The defaults are wrong for grayscale and 100-class models, so they are logged."""
    with caplog.at_level(logging.WARNING):
        infer_model_config_from_filename("model.tflite")

    warnings = [r for r in caplog.records if r.levelno == logging.WARNING]
    assert len(warnings) == 1
    assert "3-channel RGB" in warnings[0].getMessage()
    assert "10 classes" in warnings[0].getMessage()


@pytest.mark.parametrize(
    "filename", ["mnist_GRAY_10cls.tflite", "mnist_RGB_100cls.tflite"]
)
def test_infer_model_config_from_filename_is_quiet_with_tags(filename, caplog):
    with caplog.at_level(logging.WARNING):
        infer_model_config_from_filename(filename)

    assert not [r for r in caplog.records if r.levelno >= logging.WARNING]


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


def test_parse_model_txt_file_unreadable_output_shape_uses_the_filename(
    tmp_path, caplog
):
    """An output shape the parser cannot read must not leave the default silently."""
    model_path = _write_report(
        tmp_path,
        IMAGE_REPORT.replace("Output 0: [  1  10]", "Output 0: [  1   1   1 100]"),
        name="digit_100cls",
    )

    with caplog.at_level(logging.WARNING):
        config = parse_model_txt_file(model_path)

    assert config["scale_factor"] == 10.0
    assert any("class scale factor" in r.getMessage() for r in caplog.records)


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
        ("65536", 65536),
        ("65536 bytes", 65536),
        ("1GB", 1073741824),
        ("64K", 65536),
        ("1M", 1048576),
        ("2G", 2147483648),
    ],
)
def test_parse_model_txt_file_arena_units(tmp_path, reported, expected):
    model_path = _write_report(
        tmp_path,
        IMAGE_REPORT.replace("64KB", reported),
    )

    assert parse_model_txt_file(model_path)["tensor_arena_size"] == expected


@pytest.mark.parametrize("reported", ["1.5 x 64KB", "65536 words"])
def test_parse_model_txt_file_rejects_trailing_arena_text(tmp_path, reported):
    """Text the parser would drop must raise instead of picking a wrong size."""
    model_path = _write_report(tmp_path, IMAGE_REPORT.replace("64KB", reported))

    with pytest.raises(cv.Invalid, match="tensor_arena_size"):
        parse_model_txt_file(model_path)


def test_parse_model_txt_file_rejects_unreadable_arena_size(tmp_path):
    """A size the parser cannot read must not silently fall back to the default."""
    model_path = _write_report(
        tmp_path, IMAGE_REPORT.replace("64KB", "as much as it needs")
    )

    with pytest.raises(cv.Invalid, match="tensor_arena_size"):
        parse_model_txt_file(model_path)


def test_parse_model_txt_file_accepts_grayscale_input(tmp_path):
    """A 3-D input has no channel dimension, so it is a single-channel model."""
    model_path = _write_report(
        tmp_path, IMAGE_REPORT.replace("[  1  20  32   3]", "[  1  28  28]")
    )

    config = parse_model_txt_file(model_path)

    assert config["input_height"] == 28
    assert config["input_width"] == 28
    assert config["input_channels"] == 1


@pytest.mark.parametrize(
    ("parser", "report", "dtype", "key", "expected"),
    [
        (parse_model_txt_file, IMAGE_REPORT, "uint8", "input_type", "uint8"),
        (parse_audio_txt_file, AUDIO_REPORT, "int8", "feature_count", 96),
    ],
)
def test_reports_accept_the_class_repr_dtype(
    tmp_path, parser, report, dtype, key, expected
):
    """The exporter prints dtypes as <class 'numpy.uint8'> as well as numpy.uint8."""
    model_path = _write_report(
        tmp_path, report.replace(f"numpy.{dtype}", f"<class 'numpy.{dtype}'>")
    )

    assert parser(model_path)[key] == expected


def test_parse_model_txt_file_rejects_unsupported_type_without_a_shape(tmp_path):
    """The dtype check must run even when the input shape cannot be parsed."""
    model_path = _write_report(tmp_path, "Input 0: [  1  28]  numpy.int8\n")

    with pytest.raises(cv.Invalid, match="unsupported input type"):
        parse_model_txt_file(model_path)


def test_parse_model_txt_file_rejects_hybrid_without_a_shape(tmp_path):
    """Hybrid models must be rejected even when the input shape cannot be parsed."""
    model_path = _write_report(
        tmp_path,
        "Input 0: [  1  28]  numpy.float32\n<class 'numpy.int8'>\n",
    )

    with pytest.raises(cv.Invalid, match="hybrid quantization"):
        parse_model_txt_file(model_path)


@pytest.mark.parametrize("parser", [parse_model_txt_file, parse_audio_txt_file])
def test_reports_reject_delegate_operations(tmp_path, parser):
    """TFLite Micro cannot run delegates, so the model cannot work."""
    model_path = _write_report(
        tmp_path,
        (IMAGE_REPORT if parser is parse_model_txt_file else AUDIO_REPORT)
        + "Found 1 DELEGATE operation\n",
    )

    with pytest.raises(cv.Invalid, match="DELEGATE"):
        parser(model_path)


@pytest.mark.parametrize("parser", [parse_model_txt_file, parse_audio_txt_file])
def test_reports_accept_a_zero_delegate_count(tmp_path, parser):
    """A report that always prints the count says zero when there is no delegate."""
    model_path = _write_report(
        tmp_path,
        (IMAGE_REPORT if parser is parse_model_txt_file else AUDIO_REPORT)
        + "Found 0 DELEGATE operations\n",
    )

    assert parser(model_path) is not None


def test_parse_model_txt_file_reports_an_unreadable_report(tmp_path):
    """A non-UTF-8 report must name the file instead of a generic ValueError."""
    (tmp_path / "digit.txt").write_bytes(b"\xff\xfe Input 0\n")

    with pytest.raises(cv.Invalid, match="Could not read the model report") as err:
        parse_model_txt_file(tmp_path / "digit.tflite")

    assert "digit.txt" in str(err.value)


def test_parse_audio_txt_file_reports_an_invalid_cutoff(tmp_path):
    """A malformed number must name the field instead of a generic error."""
    model_path = _write_report(tmp_path, AUDIO_REPORT.replace("0.75", "0.7.5"))

    with pytest.raises(cv.Invalid, match="probability_cutoff") as err:
        parse_audio_txt_file(model_path)

    assert "digit.txt" in str(err.value)


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


def test_parse_audio_txt_file_rejects_unsupported_type_without_a_shape(tmp_path):
    """The dtype check must run even when the input shape cannot be parsed."""
    model_path = _write_report(tmp_path, "Input 0: [  1  49]  numpy.int16\n")

    with pytest.raises(cv.Invalid, match="unsupported audio input type"):
        parse_audio_txt_file(model_path)


def test_parse_audio_txt_file_rejects_hybrid_without_a_shape(tmp_path):
    """Hybrid models must be rejected even when the input shape cannot be parsed."""
    model_path = _write_report(
        tmp_path,
        "Input 0: [  1  49]  numpy.float32\n<class 'numpy.int8'>\n",
    )

    with pytest.raises(cv.Invalid, match="hybrid quantization"):
        parse_audio_txt_file(model_path)


def test_parse_audio_txt_file_rejects_delegate_without_a_shape(tmp_path):
    """A delegate model must be rejected even when the shape cannot be parsed."""
    model_path = _write_report(
        tmp_path, "Input 0: [  1  49]  numpy.int8\nFound 1 DELEGATE operation\n"
    )

    with pytest.raises(cv.Invalid, match="DELEGATE"):
        parse_audio_txt_file(model_path)


def test_parse_audio_txt_file_accepts_a_valid_4d_input(tmp_path):
    """A 4-D input like [1 49 40 1] is read like a 3-D one."""
    model_path = _write_report(
        tmp_path, AUDIO_REPORT.replace("[  1  16  96]", "[  1  49  40  1]")
    )

    config = parse_audio_txt_file(model_path)

    assert config["sliding_window_size"] == 49
    assert config["feature_count"] == 40


def test_parse_audio_txt_file_rejects_unreadable_input_section(tmp_path):
    """A report whose input section cannot be parsed leaves the YAML options in charge."""
    model_path = _write_report(
        tmp_path, AUDIO_REPORT.replace("[  1  16  96]", "garbage")
    )

    assert parse_audio_txt_file(model_path) is None


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


def test_revalidated_model_keeps_its_cached_report(tmp_path, monkeypatch, caplog):
    """A 304 means the cached report still describes the model, so a flaky
    report host must not discard it."""
    monkeypatch.setattr(model_source, "_cache_dir", lambda url: tmp_path)
    model = tmp_path / "digit.tflite"
    model.write_bytes(b"MODEL")
    report = tmp_path / "digit.txt"
    report.write_text("Input 0: [  1  28  28   1]  numpy.uint8\n", encoding="utf-8")
    report_calls = []

    def fake_download(url, path, *args, **kwargs):
        path = Path(path)
        if url.endswith(".txt"):
            report_calls.append(kwargs)
            raise OSError("network is unreachable")
        # A 304 leaves the cached model untouched.
        return path.read_bytes()

    monkeypatch.setattr("esphome.external_files.download_content", fake_download)

    with caplog.at_level(logging.WARNING, logger=model_source.__name__):
        model_source.MODEL_SOURCE_SCHEMA("https://example.com/models/digit.tflite")

    assert report_calls == [{"allow_stale": True}]
    assert report.exists()
    assert any("keeping the cached report" in r.getMessage() for r in caplog.records)


def test_unremovable_stale_report_is_an_error(tmp_path, monkeypatch):
    """Continuing would configure the model from a report of another model."""
    monkeypatch.setattr(model_source, "_cache_dir", lambda url: tmp_path)
    (tmp_path / "digit.txt").write_text(
        "Input 0: [  1  28  28   1]  numpy.uint8\n", encoding="utf-8"
    )
    download = _fake_download()

    def fake_download(url, path, *args, **kwargs):
        if url.endswith(".txt"):
            raise OSError("network is unreachable")
        return download(url, path, *args, **kwargs)

    monkeypatch.setattr("esphome.external_files.download_content", fake_download)

    real_unlink = Path.unlink

    def fake_unlink(self, *args, **kwargs):
        if self.suffix == ".txt":
            raise OSError("permission denied")
        return real_unlink(self, *args, **kwargs)

    monkeypatch.setattr(Path, "unlink", fake_unlink)

    with pytest.raises(cv.Invalid, match="could not be removed"):
        model_source.MODEL_SOURCE_SCHEMA("https://example.com/models/digit.tflite")


def test_stale_model_keeps_the_matching_cached_report(tmp_path, monkeypatch, caplog):
    """A model served from the cache must not be paired with a fresh report."""
    monkeypatch.setattr(model_source, "_cache_dir", lambda url: tmp_path)
    report = tmp_path / "digit.txt"
    report.write_text("Input 0: [  1  28  28   1]  numpy.uint8\n", encoding="utf-8")
    download = _fake_download()
    monkeypatch.setattr("esphome.external_files.download_content", download)
    monkeypatch.setattr("esphome.external_files.is_fresh_this_run", lambda path: False)

    with caplog.at_level(logging.INFO, logger=model_source.__name__):
        model_source.MODEL_SOURCE_SCHEMA("https://example.com/models/digit.tflite")

    assert [path.name for _, path in download.calls] == ["digit.tflite"]
    assert report.read_text(encoding="utf-8") == (
        "Input 0: [  1  28  28   1]  numpy.uint8\n"
    )
    assert any("could not be revalidated" in r.getMessage() for r in caplog.records)


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
