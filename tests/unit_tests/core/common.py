"""Common test utilities for core unit tests."""

from collections.abc import Callable
from pathlib import Path
from unittest.mock import patch

from esphome import config, yaml_util
from esphome.config import Config
from esphome.core import CORE


def load_config_from_yaml(
    yaml_file: Callable[[str], Path], yaml_content: str
) -> Config | None:
    """Load configuration from YAML content."""
    yaml_path = yaml_file(yaml_content)
    parsed_yaml = yaml_util.load_yaml(yaml_path)

    # Mock yaml_util.load_yaml to return our parsed content
    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed_yaml),
        patch.object(CORE, "config_path", yaml_path),
    ):
        return config.read_config({})


def load_config_from_fixture(
    yaml_file: Callable[[str], Path], fixture_name: str, fixtures_dir: Path
) -> Config | None:
    """Load configuration from a fixture file."""
    fixture_path = fixtures_dir / fixture_name
    yaml_content = fixture_path.read_text()
    return load_config_from_yaml(yaml_file, yaml_content)
