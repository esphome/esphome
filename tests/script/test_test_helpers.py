"""Unit tests for script/build_helpers.py manifest override and build helpers."""

import os
from pathlib import Path
import sys
import textwrap
from unittest.mock import MagicMock, patch

import pytest

# Add the script directory to Python path so we can import build_helpers
sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "script"))
)

import build_helpers  # noqa: E402

from esphome.loader import ComponentManifest  # noqa: E402
from tests.testing_helpers import ComponentManifestOverride  # noqa: E402

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_component_manifest(*, to_code=None, dependencies=None) -> ComponentManifest:
    mod = MagicMock()
    mod.to_code = to_code
    mod.DEPENDENCIES = dependencies or []
    return ComponentManifest(mod)


# ---------------------------------------------------------------------------
# filter_components_with_files
# ---------------------------------------------------------------------------


def test_filter_keeps_components_with_cpp_files(tmp_path: Path) -> None:
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    (comp_dir / "mycomp_test.cpp").write_text("")

    result = build_helpers.filter_components_with_files(["mycomp"], tmp_path)

    assert result == ["mycomp"]


def test_filter_keeps_components_with_h_files(tmp_path: Path) -> None:
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    (comp_dir / "helpers.h").write_text("")

    result = build_helpers.filter_components_with_files(["mycomp"], tmp_path)

    assert result == ["mycomp"]


def test_filter_drops_components_without_test_dir(tmp_path: Path) -> None:
    result = build_helpers.filter_components_with_files(["nodir"], tmp_path)

    assert result == []


def test_filter_drops_components_with_no_cpp_or_h(tmp_path: Path) -> None:
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    (comp_dir / "README.md").write_text("")

    result = build_helpers.filter_components_with_files(["mycomp"], tmp_path)

    assert result == []


# ---------------------------------------------------------------------------
# get_platform_components
# ---------------------------------------------------------------------------


def test_get_platform_components_discovers_subdirectory(tmp_path: Path) -> None:
    (tmp_path / "bthome" / "sensor").mkdir(parents=True)

    sensor_mod = MagicMock()
    sensor_mod.IS_PLATFORM_COMPONENT = True

    with patch(
        "build_helpers.get_component", return_value=ComponentManifest(sensor_mod)
    ):
        result = build_helpers.get_platform_components(["bthome"], tmp_path)

    assert result == ["sensor.bthome"]


def test_get_platform_components_skips_pycache(tmp_path: Path) -> None:
    (tmp_path / "bthome" / "__pycache__").mkdir(parents=True)

    result = build_helpers.get_platform_components(["bthome"], tmp_path)

    assert result == []


def test_get_platform_components_raises_for_invalid_domain(tmp_path: Path) -> None:
    (tmp_path / "bthome" / "notadomain").mkdir(parents=True)

    with (
        patch("build_helpers.get_component", return_value=None),
        pytest.raises(ValueError, match="notadomain"),
    ):
        build_helpers.get_platform_components(["bthome"], tmp_path)


# ---------------------------------------------------------------------------
# load_test_manifest_overrides
# ---------------------------------------------------------------------------


def test_load_suppresses_to_code(tmp_path: Path) -> None:
    """to_code is always set to None before the override is called."""

    async def real_to_code(config):
        pass

    inner = _make_component_manifest(to_code=real_to_code)

    with (
        patch("build_helpers.get_component", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)
        installed: ComponentManifestOverride = mock_set.call_args[0][1]

    assert installed.to_code is None


def test_load_calls_override_fn(tmp_path: Path) -> None:
    """override_manifest() in test_init is called with the ComponentManifestOverride."""
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    init_py = comp_dir / "__init__.py"
    init_py.write_text(
        textwrap.dedent("""\
        def override_manifest(manifest):
            manifest.dependencies = ["injected"]
    """)
    )

    inner = _make_component_manifest()
    override = ComponentManifestOverride(inner)
    override.to_code = None

    with (
        patch("build_helpers.get_component", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)
        installed: ComponentManifestOverride = mock_set.call_args[0][1]

    assert installed.dependencies == ["injected"]


def test_load_enable_codegen_in_override(tmp_path: Path) -> None:
    """An override_manifest that calls enable_codegen() restores to_code."""

    async def real_to_code(config):
        pass

    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    init_py = comp_dir / "__init__.py"
    init_py.write_text(
        textwrap.dedent("""\
        def override_manifest(manifest):
            manifest.enable_codegen()
    """)
    )

    inner = _make_component_manifest(to_code=real_to_code)

    with (
        patch("build_helpers.get_component", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)
        installed: ComponentManifestOverride = mock_set.call_args[0][1]

    assert installed.to_code is real_to_code


def test_load_no_override_file(tmp_path: Path) -> None:
    """No override file: manifest is wrapped and to_code suppressed, nothing else."""
    inner = _make_component_manifest()

    with (
        patch("build_helpers.get_component", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)

    mock_set.assert_called_once()
    key, installed = mock_set.call_args[0]
    assert key == "mycomp"
    assert isinstance(installed, ComponentManifestOverride)


def test_load_skips_already_wrapped(tmp_path: Path) -> None:
    """Components already wrapped as ComponentManifestOverride are not double-wrapped."""
    inner = _make_component_manifest()
    already_wrapped = ComponentManifestOverride(inner)

    with (
        patch("build_helpers.get_component", return_value=already_wrapped),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)

    mock_set.assert_not_called()


def test_load_skips_platform_component_already_wrapped(tmp_path: Path) -> None:
    inner = _make_component_manifest()
    already_wrapped = ComponentManifestOverride(inner)

    with (
        patch("build_helpers.get_platform", return_value=already_wrapped),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["sensor.bthome"], tmp_path)

    mock_set.assert_not_called()


def test_load_wraps_top_level_component(tmp_path: Path) -> None:
    inner = _make_component_manifest()

    with (
        patch("build_helpers.get_component", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["mycomp"], tmp_path)

    mock_set.assert_called_once()
    key, installed = mock_set.call_args[0]
    assert key == "mycomp"
    assert isinstance(installed, ComponentManifestOverride)
    assert installed.to_code is None


def test_load_wraps_platform_component(tmp_path: Path) -> None:
    inner = _make_component_manifest()

    with (
        patch("build_helpers.get_platform", return_value=inner),
        patch("build_helpers.set_testing_manifest") as mock_set,
    ):
        build_helpers.load_test_manifest_overrides(["sensor.bthome"], tmp_path)

    mock_set.assert_called_once()
    key, installed = mock_set.call_args[0]
    assert key == "bthome.sensor"
    assert isinstance(installed, ComponentManifestOverride)
    assert installed.to_code is None
