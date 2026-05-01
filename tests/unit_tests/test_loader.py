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


# ---------------------------------------------------------------------------
# recursive_sources — used only by the core "esphome" manifest so that files
# in esphome/core/<subdir>/*.cpp (e.g. esphome/core/wake/wake_host.cpp) are
# discovered without promoting <subdir>/ to a Python subpackage.
# ---------------------------------------------------------------------------


def _mock_file(filename: str) -> MagicMock:
    m = MagicMock()
    m.name = filename
    m.is_file.return_value = True
    m.is_dir.return_value = False
    return m


def _mock_dir(dirname: str, children: list, has_init: bool = False) -> MagicMock:
    """Mock a directory entry with an iterdir() and joinpath('__init__.py')."""
    d = MagicMock()
    d.name = dirname
    d.is_file.return_value = False
    d.is_dir.return_value = True
    d.iterdir.return_value = children
    init_marker = MagicMock()
    init_marker.is_file.return_value = has_init
    d.joinpath.return_value = init_marker
    return d


def test_component_manifest_resources_non_recursive_skips_subdirs() -> None:
    """Default (recursive_sources=False) does not descend into subdirectories."""
    mock_module = MagicMock()
    mock_module.__package__ = "esphome.components.test_component"
    # No FILTER_SOURCE_FILES.
    del mock_module.FILTER_SOURCE_FILES

    manifest = ComponentManifest(mock_module)  # recursive_sources defaults to False

    top_level = [
        _mock_file("top.cpp"),
        _mock_dir("subdir", [_mock_file("nested.cpp")]),
    ]
    with patch("importlib.resources.files") as mock_files_func:
        pkg = MagicMock()
        pkg.iterdir.return_value = top_level
        mock_files_func.return_value = pkg

        names = [r.resource for r in manifest.resources]

    assert names == ["top.cpp"]


def test_component_manifest_resources_recursive_walks_non_subpackage_subdirs() -> None:
    """With recursive_sources=True, a subdir without __init__.py is walked."""
    mock_module = MagicMock()
    mock_module.__package__ = "esphome.core"
    del mock_module.FILTER_SOURCE_FILES

    manifest = ComponentManifest(mock_module, recursive_sources=True)

    wake_dir = _mock_dir(
        "wake",
        [
            _mock_file("wake_host.cpp"),
            _mock_file("wake_host.h"),
            _mock_file("README.md"),  # wrong suffix, excluded
        ],
        has_init=False,
    )
    top_level = [
        _mock_file("wake.h"),
        wake_dir,
    ]
    with patch("importlib.resources.files") as mock_files_func:
        pkg = MagicMock()
        pkg.iterdir.return_value = top_level
        mock_files_func.return_value = pkg

        names = sorted(r.resource for r in manifest.resources)

    assert names == ["wake.h", "wake/wake_host.cpp", "wake/wake_host.h"]


def test_component_manifest_resources_recursive_skips_subpackages() -> None:
    """Subdirectories that ARE Python subpackages (contain __init__.py) are
    skipped even with recursive_sources=True — those load as their own
    ComponentManifest and would otherwise be double-counted."""
    mock_module = MagicMock()
    mock_module.__package__ = "esphome.components.haier"
    del mock_module.FILTER_SOURCE_FILES

    manifest = ComponentManifest(mock_module, recursive_sources=True)

    button_pkg = _mock_dir(
        "button",
        [_mock_file("self_cleaning.cpp")],
        has_init=True,  # Python subpackage — must be skipped.
    )
    top_level = [
        _mock_file("haier.cpp"),
        button_pkg,
    ]
    with patch("importlib.resources.files") as mock_files_func:
        pkg = MagicMock()
        pkg.iterdir.return_value = top_level
        mock_files_func.return_value = pkg

        names = [r.resource for r in manifest.resources]

    assert names == ["haier.cpp"]


def test_component_manifest_resources_recursive_skips_pycache() -> None:
    """__pycache__ inside a recursive walk must never be descended into."""
    mock_module = MagicMock()
    mock_module.__package__ = "esphome.core"
    del mock_module.FILTER_SOURCE_FILES

    manifest = ComponentManifest(mock_module, recursive_sources=True)

    # __pycache__ is_dir=True but must be skipped without checking __init__.py
    # or calling iterdir (would yield compiled artifacts).
    pycache = _mock_dir("__pycache__", [_mock_file("wake.cpython-314.pyc")])
    top_level = [
        _mock_file("wake.h"),
        pycache,
    ]
    with patch("importlib.resources.files") as mock_files_func:
        pkg = MagicMock()
        pkg.iterdir.return_value = top_level
        mock_files_func.return_value = pkg

        names = [r.resource for r in manifest.resources]

    assert names == ["wake.h"]


def test_component_manifest_resources_recursive_filter_source_files_supports_subpaths() -> (
    None
):
    """FILTER_SOURCE_FILES entries using '/'-joined subpaths exclude files
    inside a recursively-walked subdir."""
    mock_module = MagicMock()
    mock_module.__package__ = "esphome.core"
    mock_module.FILTER_SOURCE_FILES = lambda: ["wake/wake_host.cpp"]

    manifest = ComponentManifest(mock_module, recursive_sources=True)

    wake_dir = _mock_dir(
        "wake",
        [
            _mock_file("wake_host.cpp"),  # excluded
            _mock_file("wake_freertos.cpp"),  # kept
        ],
    )
    with patch("importlib.resources.files") as mock_files_func:
        pkg = MagicMock()
        pkg.iterdir.return_value = [wake_dir]
        mock_files_func.return_value = pkg

        names = [r.resource for r in manifest.resources]

    assert names == ["wake/wake_freertos.cpp"]
