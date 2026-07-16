"""Tests for micro_wake_word local model validation."""

import json
from pathlib import Path
from typing import Any

import pytest

from esphome.components.micro_wake_word import LOCAL_SCHEMA
from esphome.core import CORE

MANIFEST: dict[str, Any] = {
    "type": "micro",
    "model": "hey_jarvis.tflite",
    "author": "someone",
    "version": 2,
    "wake_word": "hey jarvis",
    "trained_languages": ["en"],
    "micro": {
        "feature_step_size": 10,
        "tensor_arena_size": 30000,
        "probability_cutoff": 0.97,
        "sliding_window_size": 5,
        "minimum_esphome_version": "2024.7.0",
    },
}


@pytest.fixture
def config_dir(tmp_path: Path) -> Path:
    """A config dir holding a manifest and its model file."""
    (tmp_path / "models").mkdir()
    (tmp_path / "models" / "hey_jarvis.tflite").write_bytes(b"fake model")
    (tmp_path / "models" / "hey_jarvis.json").write_text(json.dumps(MANIFEST))
    CORE.config_path = tmp_path / "test.yaml"
    return tmp_path


def test_local_schema_records_model_file(config_dir: Path) -> None:
    """The model file named by the manifest is resolved so bundles include it."""
    config = LOCAL_SCHEMA({"path": "models/hey_jarvis.json"})

    assert config["model_file"] == config_dir / "models" / "hey_jarvis.tflite"


def test_local_schema_records_model_file_in_subdirectory(config_dir: Path) -> None:
    """The model reference is resolved relative to the manifest, not the config dir."""
    nested = config_dir / "models" / "nested"
    nested.mkdir()
    (nested / "model.tflite").write_bytes(b"fake model")
    (config_dir / "models" / "nested.json").write_text(
        json.dumps({**MANIFEST, "model": "nested/model.tflite"})
    )

    config = LOCAL_SCHEMA({"path": "models/nested.json"})

    assert config["model_file"] == nested / "model.tflite"


def test_local_schema_missing_model_file_still_validates(config_dir: Path) -> None:
    """A model file that does not exist is recorded, not rejected.

    Raising here would be swallowed by the shorthand validator, which would then
    report a confusing error about a missing file in a git repository.
    """
    (config_dir / "models" / "hey_jarvis.tflite").unlink()

    config = LOCAL_SCHEMA({"path": "models/hey_jarvis.json"})

    assert config["model_file"] == config_dir / "models" / "hey_jarvis.tflite"


@pytest.mark.parametrize(
    "contents",
    [
        pytest.param("{not valid json", id="malformed"),
        pytest.param(json.dumps({"type": "micro"}), id="no_model_key"),
        pytest.param(json.dumps(["a", "list"]), id="not_an_object"),
        pytest.param(json.dumps({"model": 42}), id="model_not_a_string"),
    ],
)
def test_local_schema_bad_manifest_does_not_raise(
    config_dir: Path, contents: str
) -> None:
    """Manifest problems are left to later stages, which report them better."""
    (config_dir / "models" / "hey_jarvis.json").write_text(contents)

    config = LOCAL_SCHEMA({"path": "models/hey_jarvis.json"})

    assert "model_file" not in config
