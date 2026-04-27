"""Unit tests for esphome.loader module."""

from unittest.mock import MagicMock, patch

from esphome.loader import ComponentManifest, _replace_component_manifest, get_component
from tests.testing_helpers import ComponentManifestOverride

# ---------------------------------------------------------------------------
# ComponentManifestOverride
# ---------------------------------------------------------------------------


def _make_manifest(*, to_code=None, dependencies=None) -> ComponentManifest:
    """Return a ComponentManifest backed by a minimal mock module."""
    mod = MagicMock()
    mod.to_code = to_code
    mod.DEPENDENCIES = dependencies or []
    return ComponentManifest(mod)


def test_testing_manifest_delegates_to_wrapped() -> None:
    """Unoverridden attributes fall through to the wrapped manifest."""
    inner = _make_manifest(dependencies=["wifi"])
    tm = ComponentManifestOverride(inner)
    assert tm.dependencies == ["wifi"]


def test_testing_manifest_override_shadows_wrapped() -> None:
    """An assigned attribute shadows the wrapped value."""
    inner = _make_manifest(dependencies=["wifi"])
    tm = ComponentManifestOverride(inner)
    tm.dependencies = ["ble"]
    assert tm.dependencies == ["ble"]
    # Wrapped value unchanged
    assert inner.dependencies == ["wifi"]


def test_testing_manifest_to_code_suppression() -> None:
    """Setting to_code=None suppresses code generation."""

    async def real_to_code(config):
        pass

    inner = _make_manifest(to_code=real_to_code)
    tm = ComponentManifestOverride(inner)
    tm.to_code = None
    assert tm.to_code is None


def test_testing_manifest_enable_codegen_removes_suppression() -> None:
    """enable_codegen() removes the to_code override, restoring the original."""

    async def real_to_code(config):
        pass

    inner = _make_manifest(to_code=real_to_code)
    tm = ComponentManifestOverride(inner)
    tm.to_code = None
    assert tm.to_code is None

    tm.enable_codegen()
    assert tm.to_code is real_to_code


def test_testing_manifest_enable_codegen_preserves_other_overrides() -> None:
    """enable_codegen() only removes to_code; other overrides survive."""
    inner = _make_manifest(dependencies=["wifi"])
    tm = ComponentManifestOverride(inner)
    tm.to_code = None
    tm.dependencies = ["ble"]

    tm.enable_codegen()

    assert tm.to_code is inner.to_code
    assert tm.dependencies == ["ble"]


def test_testing_manifest_restore_clears_all_overrides() -> None:
    """restore() removes every override, reverting all attributes to wrapped values."""

    async def real_to_code(config):
        pass

    inner = _make_manifest(to_code=real_to_code, dependencies=["wifi"])
    tm = ComponentManifestOverride(inner)
    tm.to_code = None
    tm.dependencies = ["ble"]

    tm.restore()

    assert tm.to_code is real_to_code
    assert tm.dependencies == ["wifi"]


def test_replace_component_manifest_installs_override() -> None:
    """_replace_component_manifest replaces the cached manifest for a domain."""
    inner = _make_manifest()
    override = ComponentManifestOverride(inner)

    _replace_component_manifest("_test_dummy_domain", override)

    assert get_component("_test_dummy_domain") is override


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
