"""Shared fixtures for core unit tests."""

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.fixture
def yaml_file(tmp_path: Path) -> Callable[[str], Path]:
    """Create a temporary YAML file for testing."""

    def _yaml_file(content: str) -> Path:
        yaml_path = tmp_path / "test.yaml"
        yaml_path.write_text(content)
        return yaml_path

    return _yaml_file
