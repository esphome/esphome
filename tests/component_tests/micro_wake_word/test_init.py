"""Tests for micro_wake_word local model validation."""

import json
import logging
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


def _registered_files() -> list[Path]:
    """Files components registered for bundling this run."""
    data = CORE.data.get("bundle")
    return list(data.extra_files) if data else []


@pytest.fixture
def config_dir(tmp_path: Path) -> Path:
    """A config dir holding a manifest and its model file."""
    (tmp_path / "models").mkdir()
    (tmp_path / "models" / "hey_jarvis.tflite").write_bytes(b"fake model")
    (tmp_path / "models" / "hey_jarvis.json").write_text(json.dumps(MANIFEST))
    CORE.config_path = tmp_path / "test.yaml"
    return tmp_path


def test_local_schema_registers_model_file(config_dir: Path) -> None:
    """The model file named by the manifest is registered so bundles include it."""
    LOCAL_SCHEMA({"path": "models/hey_jarvis.json"})

    assert _registered_files() == [config_dir / "models" / "hey_jarvis.tflite"]


def test_local_schema_registers_model_file_in_subdirectory(config_dir: Path) -> None:
    """The model reference is resolved relative to the manifest, not the config dir."""
    nested = config_dir / "models" / "nested"
    nested.mkdir()
    (nested / "model.tflite").write_bytes(b"fake model")
    (config_dir / "models" / "nested.json").write_text(
        json.dumps({**MANIFEST, "model": "nested/model.tflite"})
    )

    LOCAL_SCHEMA({"path": "models/nested.json"})

    assert _registered_files() == [nested / "model.tflite"]


def test_local_schema_leaves_config_untouched(config_dir: Path) -> None:
    """Registration is a side effect; the model file is not a config key."""
    config = LOCAL_SCHEMA({"path": "models/hey_jarvis.json"})

    assert config == {"path": config_dir / "models" / "hey_jarvis.json"}


def test_local_schema_missing_model_file_still_validates(config_dir: Path) -> None:
    """A model file that does not exist is registered, not rejected.

    Raising here would be swallowed by the shorthand validator, which would then
    report a confusing error about a missing file in a git repository.
    """
    (config_dir / "models" / "hey_jarvis.tflite").unlink()

    LOCAL_SCHEMA({"path": "models/hey_jarvis.json"})

    assert _registered_files() == [config_dir / "models" / "hey_jarvis.tflite"]


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
    config_dir: Path, contents: str, caplog: pytest.LogCaptureFixture
) -> None:
    """Manifest problems are left to later stages, which report them better.

    The skipped registration is logged so a bundle built without the model file can
    be diagnosed.
    """
    (config_dir / "models" / "hey_jarvis.json").write_text(contents)

    with caplog.at_level(logging.DEBUG):
        LOCAL_SCHEMA({"path": "models/hey_jarvis.json"})

    assert _registered_files() == []
    assert "Not registering a model file" in caplog.text
