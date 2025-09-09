"""Shared fixtures for core unit tests."""

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.fixture
def yaml_file(tmp_path: Path) -> Callable[[str], str]:
    """Create a temporary YAML file for testing."""

    def _yaml_file(content: str) -> str:
        yaml_path = tmp_path / "test.yaml"
        yaml_path.write_text(content)
        return str(yaml_path)

    return _yaml_file
