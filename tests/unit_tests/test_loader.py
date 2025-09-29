"""Unit tests for esphome.loader module."""

from unittest.mock import MagicMock, patch

from esphome.loader import ComponentManifest


def test_component_manifest_resources_with_filter_source_files() -> None:
    """Test that ComponentManifest.resources correctly filters out excluded files."""
    # Create a mock module with FILTER_SOURCE_FILES function
    mock_module = MagicMock()
    mock_module.FILTER_SOURCE_FILES = lambda: [
        "platform_esp32.cpp",
        "platform_esp8266.cpp",
    ]
    mock_module.__package__ = "esphome.components.test_component"

    # Create ComponentManifest instance
    manifest = ComponentManifest(mock_module)

    # Mock the files in the package
    def create_mock_file(filename: str) -> MagicMock:
        mock_file = MagicMock()
        mock_file.name = filename
        mock_file.is_file.return_value = True
        return mock_file

    mock_files = [
        create_mock_file("test.cpp"),
        create_mock_file("test.h"),
        create_mock_file("platform_esp32.cpp"),
        create_mock_file("platform_esp8266.cpp"),
        create_mock_file("common.cpp"),
        create_mock_file("README.md"),  # Should be excluded by extension
    ]

    # Mock importlib.resources
    with patch("importlib.resources.files") as mock_files_func:
        mock_package_files = MagicMock()
        mock_package_files.iterdir.return_value = mock_files
        mock_package_files.joinpath = lambda name: MagicMock(is_file=lambda: True)
        mock_files_func.return_value = mock_package_files

        # Get resources
        resources = manifest.resources

        # Convert to list of filenames for easier testing
        resource_names = [r.resource for r in resources]

        # Check that platform files are excluded
        assert "platform_esp32.cpp" not in resource_names
        assert "platform_esp8266.cpp" not in resource_names

        # Check that other source files are included
        assert "test.cpp" in resource_names
        assert "test.h" in resource_names
        assert "common.cpp" in resource_names

        # Check that non-source files are excluded
        assert "README.md" not in resource_names

        # Verify the correct number of resources
        assert len(resources) == 3  # test.cpp, test.h, common.cpp
