import pytest

from pathlib import Path

here = Path(__file__).parent


@pytest.fixture
def fixture_path() -> Path:
    """
    Location of all fixture files.
    """
    return here / "fixtures"
