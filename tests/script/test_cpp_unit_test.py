"""Unit tests for script/cpp_unit_test.py."""

import os
from pathlib import Path
import sys
import textwrap
from unittest.mock import MagicMock, patch

import pytest

# Add the script directory to Python path so we can import cpp_unit_test
sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "script"))
)

import cpp_unit_test  # noqa: E402

from esphome.loader import ComponentManifest, TestingComponentManifest  # noqa: E402

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_component_manifest(*, to_code=None, dependencies=None) -> ComponentManifest:
    mod = MagicMock()
    mod.to_code = to_code
    mod.DEPENDENCIES = dependencies or []
    return ComponentManifest(mod)


# ---------------------------------------------------------------------------
# filter_components_without_tests
# ---------------------------------------------------------------------------


def test_filter_keeps_components_with_cpp_files(tmp_path: Path) -> None:
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    (comp_dir / "mycomp_test.cpp").write_text("")

    with patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path):
        result = cpp_unit_test.filter_components_without_tests(["mycomp"])

    assert result == ["mycomp"]


def test_filter_keeps_components_with_h_files(tmp_path: Path) -> None:
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    (comp_dir / "helpers.h").write_text("")

    with patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path):
        result = cpp_unit_test.filter_components_without_tests(["mycomp"])

    assert result == ["mycomp"]


def test_filter_drops_components_without_test_dir(tmp_path: Path) -> None:
    with patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path):
        result = cpp_unit_test.filter_components_without_tests(["nodir"])

    assert result == []


def test_filter_drops_components_with_no_cpp_or_h(tmp_path: Path) -> None:
    comp_dir = tmp_path / "mycomp"
    comp_dir.mkdir()
    (comp_dir / "README.md").write_text("")

    with patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path):
        result = cpp_unit_test.filter_components_without_tests(["mycomp"])

    assert result == []


# ---------------------------------------------------------------------------
# get_platform_components
# ---------------------------------------------------------------------------


def test_get_platform_components_discovers_subdirectory(tmp_path: Path) -> None:
    (tmp_path / "bthome" / "sensor").mkdir(parents=True)

    sensor_mod = MagicMock()
    sensor_mod.IS_PLATFORM_COMPONENT = True

    with (
        patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path),
        patch(
            "cpp_unit_test.get_component", return_value=ComponentManifest(sensor_mod)
        ),
    ):
        result = cpp_unit_test.get_platform_components(["bthome"])

    assert result == ["sensor.bthome"]


def test_get_platform_components_skips_pycache(tmp_path: Path) -> None:
    (tmp_path / "bthome" / "__pycache__").mkdir(parents=True)

    with patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path):
        result = cpp_unit_test.get_platform_components(["bthome"])

    assert result == []


def test_get_platform_components_raises_for_invalid_domain(tmp_path: Path) -> None:
    (tmp_path / "bthome" / "notadomain").mkdir(parents=True)

    with (
        patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path),
        patch("cpp_unit_test.get_component", return_value=None),
        pytest.raises(ValueError, match="notadomain"),
    ):
        cpp_unit_test.get_platform_components(["bthome"])


# ---------------------------------------------------------------------------
# _build_testing_manifest
# ---------------------------------------------------------------------------


def test_build_testing_manifest_suppresses_to_code(tmp_path: Path) -> None:
    """to_code is always set to None before the override is called."""

    async def real_to_code(config):
        pass

    inner = _make_component_manifest(to_code=real_to_code)

    with patch("cpp_unit_test.set_testing_manifest") as mock_set:
        cpp_unit_test._build_testing_manifest(
            inner,
            cache_key="mycomp",
            test_init=tmp_path / "nonexistent.py",
        )
        installed: TestingComponentManifest = mock_set.call_args[0][1]

    assert installed.to_code is None


def test_build_testing_manifest_calls_override_fn(tmp_path: Path) -> None:
    """override_manifest() in test_init is called with the TestingComponentManifest."""
    init_py = tmp_path / "__init__.py"
    init_py.write_text(
        textwrap.dedent("""\
        def override_manifest(manifest):
            manifest.dependencies = ["injected"]
    """)
    )

    inner = _make_component_manifest()

    with patch("cpp_unit_test.set_testing_manifest") as mock_set:
        cpp_unit_test._build_testing_manifest(
            inner,
            cache_key="mycomp",
            test_init=init_py,
        )
        installed: TestingComponentManifest = mock_set.call_args[0][1]

    assert installed.dependencies == ["injected"]


def test_build_testing_manifest_enable_codegen_in_override(tmp_path: Path) -> None:
    """An override_manifest that calls enable_codegen() restores to_code."""

    async def real_to_code(config):
        pass

    init_py = tmp_path / "__init__.py"
    init_py.write_text(
        textwrap.dedent("""\
        def override_manifest(manifest):
            manifest.enable_codegen()
    """)
    )

    inner = _make_component_manifest(to_code=real_to_code)

    with patch("cpp_unit_test.set_testing_manifest") as mock_set:
        cpp_unit_test._build_testing_manifest(
            inner,
            cache_key="mycomp",
            test_init=init_py,
        )
        installed: TestingComponentManifest = mock_set.call_args[0][1]

    assert installed.to_code is real_to_code


def test_build_testing_manifest_no_override_file(tmp_path: Path) -> None:
    """No override file: manifest is wrapped and to_code suppressed, nothing else."""
    inner = _make_component_manifest()

    with patch("cpp_unit_test.set_testing_manifest") as mock_set:
        cpp_unit_test._build_testing_manifest(
            inner,
            cache_key="mycomp",
            test_init=tmp_path / "nonexistent.py",
        )
        key, installed = mock_set.call_args[0]

    assert key == "mycomp"
    assert isinstance(installed, TestingComponentManifest)


# ---------------------------------------------------------------------------
# load_test_manifest_overrides
# ---------------------------------------------------------------------------


def test_load_skips_already_wrapped(tmp_path: Path) -> None:
    """Components already wrapped as TestingComponentManifest are not double-wrapped."""
    inner = _make_component_manifest()
    already_wrapped = TestingComponentManifest(inner)

    with (
        patch("cpp_unit_test.get_component", return_value=already_wrapped),
        patch("cpp_unit_test.set_testing_manifest") as mock_set,
        patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path),
    ):
        cpp_unit_test.load_test_manifest_overrides(["mycomp"])

    mock_set.assert_not_called()


def test_load_skips_platform_component_already_wrapped(tmp_path: Path) -> None:
    inner = _make_component_manifest()
    already_wrapped = TestingComponentManifest(inner)

    with (
        patch("cpp_unit_test.get_platform", return_value=already_wrapped),
        patch("cpp_unit_test.set_testing_manifest") as mock_set,
        patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path),
    ):
        cpp_unit_test.load_test_manifest_overrides(["sensor.bthome"])

    mock_set.assert_not_called()


def test_load_wraps_top_level_component(tmp_path: Path) -> None:
    inner = _make_component_manifest()

    with (
        patch("cpp_unit_test.get_component", return_value=inner),
        patch("cpp_unit_test.set_testing_manifest") as mock_set,
        patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path),
    ):
        cpp_unit_test.load_test_manifest_overrides(["mycomp"])

    mock_set.assert_called_once()
    key, installed = mock_set.call_args[0]
    assert key == "mycomp"
    assert isinstance(installed, TestingComponentManifest)
    assert installed.to_code is None


def test_load_wraps_platform_component(tmp_path: Path) -> None:
    inner = _make_component_manifest()

    with (
        patch("cpp_unit_test.get_platform", return_value=inner),
        patch("cpp_unit_test.set_testing_manifest") as mock_set,
        patch.object(cpp_unit_test, "COMPONENTS_TESTS_DIR", tmp_path),
    ):
        cpp_unit_test.load_test_manifest_overrides(["sensor.bthome"])

    mock_set.assert_called_once()
    key, installed = mock_set.call_args[0]
    assert key == "bthome.sensor"
    assert isinstance(installed, TestingComponentManifest)
    assert installed.to_code is None
