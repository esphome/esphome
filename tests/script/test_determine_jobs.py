"""Unit tests for script/determine-jobs.py module."""

from collections.abc import Generator
import importlib.util
import json
import os
from pathlib import Path
import sys
from unittest.mock import Mock, call, patch

import pytest

# Add the script directory to Python path so we can import the module
script_dir = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "script")
)
sys.path.insert(0, script_dir)

# Import helpers module for patching
import helpers  # noqa: E402

spec = importlib.util.spec_from_file_location(
    "determine_jobs", os.path.join(script_dir, "determine-jobs.py")
)
determine_jobs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(determine_jobs)


@pytest.fixture
def mock_should_run_integration_tests() -> Generator[Mock, None, None]:
    """Mock should_run_integration_tests from helpers."""
    with patch.object(determine_jobs, "should_run_integration_tests") as mock:
        yield mock


@pytest.fixture
def mock_should_run_clang_tidy() -> Generator[Mock, None, None]:
    """Mock should_run_clang_tidy from helpers."""
    with patch.object(determine_jobs, "should_run_clang_tidy") as mock:
        yield mock


@pytest.fixture
def mock_should_run_clang_format() -> Generator[Mock, None, None]:
    """Mock should_run_clang_format from helpers."""
    with patch.object(determine_jobs, "should_run_clang_format") as mock:
        yield mock


@pytest.fixture
def mock_should_run_python_linters() -> Generator[Mock, None, None]:
    """Mock should_run_python_linters from helpers."""
    with patch.object(determine_jobs, "should_run_python_linters") as mock:
        yield mock


@pytest.fixture
def mock_determine_cpp_unit_tests() -> Generator[Mock, None, None]:
    """Mock determine_cpp_unit_tests from helpers."""
    with patch.object(determine_jobs, "determine_cpp_unit_tests") as mock:
        yield mock


@pytest.fixture
def mock_changed_files() -> Generator[Mock, None, None]:
    """Mock changed_files for memory impact detection."""
    with patch.object(determine_jobs, "changed_files") as mock:
        # Default to empty list
        mock.return_value = []
        yield mock


@pytest.fixture(autouse=True)
def clear_clang_tidy_cache() -> None:
    """Clear the clang-tidy full scan cache before each test."""
    determine_jobs._is_clang_tidy_full_scan.cache_clear()


def test_main_all_tests_should_run(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_changed_files: Mock,
    mock_determine_cpp_unit_tests: Mock,
    capsys: pytest.CaptureFixture[str],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Test when all tests should run."""
    # Ensure we're not in GITHUB_ACTIONS mode for this test
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)

    mock_should_run_integration_tests.return_value = True
    mock_should_run_clang_tidy.return_value = True
    mock_should_run_clang_format.return_value = True
    mock_should_run_python_linters.return_value = True
    mock_determine_cpp_unit_tests.return_value = (False, ["wifi", "api", "sensor"])

    # Mock changed_files to return non-component files (to avoid memory impact)
    # Memory impact only runs when component C++ files change
    mock_changed_files.return_value = [
        "esphome/config.py",
        "esphome/helpers.py",
    ]

    # Run main function with mocked argv
    with (
        patch("sys.argv", ["determine-jobs.py"]),
        patch.object(determine_jobs, "_is_clang_tidy_full_scan", return_value=False),
        patch.object(
            determine_jobs,
            "get_changed_components",
            return_value=["wifi", "api", "sensor"],
        ),
        patch.object(
            determine_jobs,
            "filter_component_and_test_files",
            side_effect=lambda f: f.startswith("esphome/components/"),
        ),
        patch.object(
            determine_jobs,
            "get_components_with_dependencies",
            side_effect=lambda files, deps: (
                ["wifi", "api"] if not deps else ["wifi", "api", "sensor"]
            ),
        ),
    ):
        determine_jobs.main()

    # Check output
    captured = capsys.readouterr()
    output = json.loads(captured.out)

    assert output["integration_tests"] is True
    assert output["clang_tidy"] is True
    assert output["clang_tidy_mode"] in ["nosplit", "split"]
    assert output["clang_format"] is True
    assert output["python_linters"] is True
    assert output["changed_components"] == ["wifi", "api", "sensor"]
    # changed_components_with_tests will only include components that actually have test files
    assert "changed_components_with_tests" in output
    assert isinstance(output["changed_components_with_tests"], list)
    # component_test_count matches number of components with tests
    assert output["component_test_count"] == len(
        output["changed_components_with_tests"]
    )
    # changed_cpp_file_count should be present
    assert "changed_cpp_file_count" in output
    assert isinstance(output["changed_cpp_file_count"], int)
    # memory_impact should be false (no component C++ files changed)
    assert "memory_impact" in output
    assert output["memory_impact"]["should_run"] == "false"
    assert output["cpp_unit_tests_run_all"] is False
    assert output["cpp_unit_tests_components"] == ["wifi", "api", "sensor"]


def test_main_no_tests_should_run(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_changed_files: Mock,
    mock_determine_cpp_unit_tests: Mock,
    capsys: pytest.CaptureFixture[str],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Test when no tests should run."""
    # Ensure we're not in GITHUB_ACTIONS mode for this test
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)

    mock_should_run_integration_tests.return_value = False
    mock_should_run_clang_tidy.return_value = False
    mock_should_run_clang_format.return_value = False
    mock_should_run_python_linters.return_value = False
    mock_determine_cpp_unit_tests.return_value = (False, [])

    # Mock changed_files to return no component files
    mock_changed_files.return_value = []

    # Run main function with mocked argv
    with (
        patch("sys.argv", ["determine-jobs.py"]),
        patch.object(determine_jobs, "get_changed_components", return_value=[]),
        patch.object(
            determine_jobs, "filter_component_and_test_files", return_value=False
        ),
        patch.object(
            determine_jobs, "get_components_with_dependencies", return_value=[]
        ),
    ):
        determine_jobs.main()

    # Check output
    captured = capsys.readouterr()
    output = json.loads(captured.out)

    assert output["integration_tests"] is False
    assert output["clang_tidy"] is False
    assert output["clang_tidy_mode"] == "disabled"
    assert output["clang_format"] is False
    assert output["python_linters"] is False
    assert output["changed_components"] == []
    assert output["changed_components_with_tests"] == []
    assert output["component_test_count"] == 0
    # changed_cpp_file_count should be 0
    assert output["changed_cpp_file_count"] == 0
    # memory_impact should be present
    assert "memory_impact" in output
    assert output["memory_impact"]["should_run"] == "false"
    assert output["cpp_unit_tests_run_all"] is False
    assert output["cpp_unit_tests_components"] == []


def test_main_with_branch_argument(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_changed_files: Mock,
    mock_determine_cpp_unit_tests: Mock,
    capsys: pytest.CaptureFixture[str],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Test with branch argument."""
    # Ensure we're not in GITHUB_ACTIONS mode for this test
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)

    mock_should_run_integration_tests.return_value = False
    mock_should_run_clang_tidy.return_value = True
    mock_should_run_clang_format.return_value = False
    mock_should_run_python_linters.return_value = True
    mock_determine_cpp_unit_tests.return_value = (False, ["mqtt"])

    # Mock changed_files to return non-component files (to avoid memory impact)
    # Memory impact only runs when component C++ files change
    mock_changed_files.return_value = ["esphome/config.py"]

    with (
        patch("sys.argv", ["script.py", "-b", "main"]),
        patch.object(determine_jobs, "_is_clang_tidy_full_scan", return_value=False),
        patch.object(determine_jobs, "get_changed_components", return_value=["mqtt"]),
        patch.object(
            determine_jobs,
            "filter_component_and_test_files",
            side_effect=lambda f: f.startswith("esphome/components/"),
        ),
        patch.object(
            determine_jobs, "get_components_with_dependencies", return_value=["mqtt"]
        ),
    ):
        determine_jobs.main()

    # Check that functions were called with branch
    mock_should_run_integration_tests.assert_called_once_with("main")
    mock_should_run_clang_tidy.assert_called_once_with("main")
    mock_should_run_clang_format.assert_called_once_with("main")
    mock_should_run_python_linters.assert_called_once_with("main")

    # Check output
    captured = capsys.readouterr()
    output = json.loads(captured.out)

    assert output["integration_tests"] is False
    assert output["clang_tidy"] is True
    assert output["clang_tidy_mode"] in ["nosplit", "split"]
    assert output["clang_format"] is False
    assert output["python_linters"] is True
    assert output["changed_components"] == ["mqtt"]
    # changed_components_with_tests will only include components that actually have test files
    assert "changed_components_with_tests" in output
    assert isinstance(output["changed_components_with_tests"], list)
    # component_test_count matches number of components with tests
    assert output["component_test_count"] == len(
        output["changed_components_with_tests"]
    )
    # changed_cpp_file_count should be present
    assert "changed_cpp_file_count" in output
    assert isinstance(output["changed_cpp_file_count"], int)
    # memory_impact should be false (no component C++ files changed)
    assert "memory_impact" in output
    assert output["memory_impact"]["should_run"] == "false"
    assert output["cpp_unit_tests_run_all"] is False
    assert output["cpp_unit_tests_components"] == ["mqtt"]


def test_should_run_integration_tests(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Test should_run_integration_tests function."""
    # Core C++ files trigger tests
    with patch.object(
        determine_jobs, "changed_files", return_value=["esphome/core/component.cpp"]
    ):
        result = determine_jobs.should_run_integration_tests()
        assert result is True

    # Core Python files trigger tests
    with patch.object(
        determine_jobs, "changed_files", return_value=["esphome/core/config.py"]
    ):
        result = determine_jobs.should_run_integration_tests()
        assert result is True

    # Python files directly in esphome/ do NOT trigger tests
    with patch.object(
        determine_jobs, "changed_files", return_value=["esphome/config.py"]
    ):
        result = determine_jobs.should_run_integration_tests()
        assert result is False

    # Python files in subdirectories (not core) do NOT trigger tests
    with patch.object(
        determine_jobs,
        "changed_files",
        return_value=["esphome/dashboard/web_server.py"],
    ):
        result = determine_jobs.should_run_integration_tests()
        assert result is False


def test_should_run_integration_tests_with_branch() -> None:
    """Test should_run_integration_tests with branch argument."""
    with patch.object(determine_jobs, "changed_files") as mock_changed:
        mock_changed.return_value = []
        determine_jobs.should_run_integration_tests("release")
        mock_changed.assert_called_once_with("release")


def test_should_run_integration_tests_component_dependency() -> None:
    """Test that integration tests run when components used in fixtures change."""
    with (
        patch.object(
            determine_jobs,
            "changed_files",
            return_value=["esphome/components/api/api.cpp"],
        ),
        patch.object(
            determine_jobs, "get_components_from_integration_fixtures"
        ) as mock_fixtures,
    ):
        mock_fixtures.return_value = {"api", "sensor"}
        with patch.object(determine_jobs, "get_all_dependencies") as mock_deps:
            mock_deps.return_value = {"api", "sensor", "network"}
            result = determine_jobs.should_run_integration_tests()
            assert result is True


@pytest.mark.parametrize(
    ("check_returncode", "changed_files", "expected_result"),
    [
        (0, [], True),  # Hash changed - need full scan
        (1, ["esphome/core.cpp"], True),  # C++ file changed
        (1, ["README.md"], False),  # No C++ files changed
        (1, [".clang-tidy.hash"], True),  # Hash file itself changed
        (1, ["platformio.ini", ".clang-tidy.hash"], True),  # Config + hash changed
    ],
)
def test_should_run_clang_tidy(
    check_returncode: int,
    changed_files: list[str],
    expected_result: bool,
) -> None:
    """Test should_run_clang_tidy function."""
    with (
        patch.object(determine_jobs, "changed_files", return_value=changed_files),
        patch("subprocess.run") as mock_run,
    ):
        # Test with hash check returning specific code
        mock_run.return_value = Mock(returncode=check_returncode)
        result = determine_jobs.should_run_clang_tidy()
        assert result == expected_result


def test_should_run_clang_tidy_hash_check_exception() -> None:
    """Test should_run_clang_tidy when hash check fails with exception."""
    # When hash check fails, clang-tidy should run as a safety measure
    with (
        patch.object(determine_jobs, "changed_files", return_value=["README.md"]),
        patch("subprocess.run", side_effect=Exception("Hash check failed")),
    ):
        result = determine_jobs.should_run_clang_tidy()
        assert result is True  # Fail safe - run clang-tidy


def test_should_run_clang_tidy_with_branch() -> None:
    """Test should_run_clang_tidy with branch argument."""
    with patch.object(determine_jobs, "changed_files") as mock_changed:
        mock_changed.return_value = []
        with patch("subprocess.run") as mock_run:
            mock_run.return_value = Mock(returncode=1)  # Hash unchanged
            determine_jobs.should_run_clang_tidy("release")
            # Changed files is called twice now - once for hash check, once for .clang-tidy.hash check
            assert mock_changed.call_count == 2
            mock_changed.assert_has_calls([call("release"), call("release")])


@pytest.mark.parametrize(
    ("changed_files", "expected_result"),
    [
        (["esphome/core.py"], True),
        (["script/test.py"], True),
        (["esphome/test.pyi"], True),  # .pyi files should trigger
        (["README.md"], False),
        ([], False),
    ],
)
def test_should_run_python_linters(
    changed_files: list[str], expected_result: bool
) -> None:
    """Test should_run_python_linters function."""
    with patch.object(determine_jobs, "changed_files", return_value=changed_files):
        result = determine_jobs.should_run_python_linters()
        assert result == expected_result


def test_should_run_python_linters_with_branch() -> None:
    """Test should_run_python_linters with branch argument."""
    with patch.object(determine_jobs, "changed_files") as mock_changed:
        mock_changed.return_value = []
        determine_jobs.should_run_python_linters("release")
        mock_changed.assert_called_once_with("release")


@pytest.mark.parametrize(
    ("changed_files", "expected_result"),
    [
        (["esphome/core.cpp"], True),
        (["esphome/core.h"], True),
        (["test.hpp"], True),
        (["test.cc"], True),
        (["test.cxx"], True),
        (["test.c"], True),
        (["test.tcc"], True),
        (["README.md"], False),
        ([], False),
    ],
)
def test_should_run_clang_format(
    changed_files: list[str], expected_result: bool
) -> None:
    """Test should_run_clang_format function."""
    with patch.object(determine_jobs, "changed_files", return_value=changed_files):
        result = determine_jobs.should_run_clang_format()
        assert result == expected_result


def test_should_run_clang_format_with_branch() -> None:
    """Test should_run_clang_format with branch argument."""
    with patch.object(determine_jobs, "changed_files") as mock_changed:
        mock_changed.return_value = []
        determine_jobs.should_run_clang_format("release")
        mock_changed.assert_called_once_with("release")


@pytest.mark.parametrize(
    ("changed_files", "expected_count"),
    [
        (["esphome/core.cpp"], 1),
        (["esphome/core.h"], 1),
        (["test.hpp"], 1),
        (["test.cc"], 1),
        (["test.cxx"], 1),
        (["test.c"], 1),
        (["test.tcc"], 1),
        (["esphome/core.cpp", "esphome/core.h"], 2),
        (["esphome/core.cpp", "esphome/core.h", "test.cc"], 3),
        (["README.md"], 0),
        (["esphome/config.py"], 0),
        (["README.md", "esphome/config.py"], 0),
        (["esphome/core.cpp", "README.md", "esphome/config.py"], 1),
        ([], 0),
    ],
)
def test_count_changed_cpp_files(changed_files: list[str], expected_count: int) -> None:
    """Test count_changed_cpp_files function."""
    with patch.object(determine_jobs, "changed_files", return_value=changed_files):
        result = determine_jobs.count_changed_cpp_files()
        assert result == expected_count


def test_count_changed_cpp_files_with_branch() -> None:
    """Test count_changed_cpp_files with branch argument."""
    with patch.object(determine_jobs, "changed_files") as mock_changed:
        mock_changed.return_value = []
        determine_jobs.count_changed_cpp_files("release")
        mock_changed.assert_called_once_with("release")


def test_main_filters_components_without_tests(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_changed_files: Mock,
    capsys: pytest.CaptureFixture[str],
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Test that components without test files are filtered out."""
    # Ensure we're not in GITHUB_ACTIONS mode for this test
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)

    mock_should_run_integration_tests.return_value = False
    mock_should_run_clang_tidy.return_value = False
    mock_should_run_clang_format.return_value = False
    mock_should_run_python_linters.return_value = False

    # Mock changed_files to return component files
    mock_changed_files.return_value = [
        "esphome/components/wifi/wifi.cpp",
        "esphome/components/sensor/sensor.h",
    ]

    # Create test directory structure
    tests_dir = tmp_path / "tests" / "components"

    # wifi has tests
    wifi_dir = tests_dir / "wifi"
    wifi_dir.mkdir(parents=True)
    (wifi_dir / "test.esp32.yaml").write_text("test: config")

    # sensor has tests
    sensor_dir = tests_dir / "sensor"
    sensor_dir.mkdir(parents=True)
    (sensor_dir / "test.esp8266.yaml").write_text("test: config")

    # airthings_ble exists but has no test files
    airthings_dir = tests_dir / "airthings_ble"
    airthings_dir.mkdir(parents=True)

    # Mock root_path to use tmp_path (need to patch both determine_jobs and helpers)
    with (
        patch.object(determine_jobs, "root_path", str(tmp_path)),
        patch.object(helpers, "root_path", str(tmp_path)),
        patch("sys.argv", ["determine-jobs.py"]),
        patch.object(
            determine_jobs,
            "get_changed_components",
            return_value=["wifi", "sensor", "airthings_ble"],
        ),
        patch.object(
            determine_jobs,
            "filter_component_and_test_files",
            side_effect=lambda f: f.startswith("esphome/components/"),
        ),
        patch.object(
            determine_jobs,
            "get_components_with_dependencies",
            side_effect=lambda files, deps: (
                ["wifi", "sensor"] if not deps else ["wifi", "sensor", "airthings_ble"]
            ),
        ),
        patch.object(determine_jobs, "changed_files", return_value=[]),
    ):
        # Clear the cache since we're mocking root_path
        determine_jobs._component_has_tests.cache_clear()
        determine_jobs.main()

    # Check output
    captured = capsys.readouterr()
    output = json.loads(captured.out)

    # changed_components should have all components
    assert set(output["changed_components"]) == {"wifi", "sensor", "airthings_ble"}
    # changed_components_with_tests should only have components with test files
    assert set(output["changed_components_with_tests"]) == {"wifi", "sensor"}
    # component_test_count should be based on components with tests
    assert output["component_test_count"] == 2
    # changed_cpp_file_count should be present
    assert "changed_cpp_file_count" in output
    assert isinstance(output["changed_cpp_file_count"], int)
    # memory_impact should be present
    assert "memory_impact" in output
    assert output["memory_impact"]["should_run"] == "false"


# Tests for detect_memory_impact_config function


def test_detect_memory_impact_config_with_common_platform(tmp_path: Path) -> None:
    """Test memory impact detection when components share a common platform."""
    # Create test directory structure
    tests_dir = tmp_path / "tests" / "components"

    # wifi component with esp32-idf test
    wifi_dir = tests_dir / "wifi"
    wifi_dir.mkdir(parents=True)
    (wifi_dir / "test.esp32-idf.yaml").write_text("test: wifi")

    # api component with esp32-idf test
    api_dir = tests_dir / "api"
    api_dir.mkdir(parents=True)
    (api_dir / "test.esp32-idf.yaml").write_text("test: api")

    # Mock changed_files to return wifi and api component changes
    with (
        patch.object(determine_jobs, "root_path", str(tmp_path)),
        patch.object(helpers, "root_path", str(tmp_path)),
        patch.object(determine_jobs, "changed_files") as mock_changed_files,
    ):
        mock_changed_files.return_value = [
            "esphome/components/wifi/wifi.cpp",
            "esphome/components/api/api.cpp",
        ]
        determine_jobs._component_has_tests.cache_clear()

        result = determine_jobs.detect_memory_impact_config()

    assert result["should_run"] == "true"
    assert set(result["components"]) == {"wifi", "api"}
    assert result["platform"] == "esp32-idf"  # Common platform
    assert result["use_merged_config"] == "true"


def test_detect_memory_impact_config_core_only_changes(tmp_path: Path) -> None:
    """Test memory impact detection with core C++ changes (no component changes)."""
    # Create test directory structure with fallback component
    tests_dir = tmp_path / "tests" / "components"

    # api component (fallback component) with esp32-idf test
    api_dir = tests_dir / "api"
    api_dir.mkdir(parents=True)
    (api_dir / "test.esp32-idf.yaml").write_text("test: api")

    # Mock changed_files to return only core C++ files (no component files)
    with (
        patch.object(determine_jobs, "root_path", str(tmp_path)),
        patch.object(helpers, "root_path", str(tmp_path)),
        patch.object(determine_jobs, "changed_files") as mock_changed_files,
    ):
        mock_changed_files.return_value = [
            "esphome/core/application.cpp",
            "esphome/core/component.h",
        ]
        determine_jobs._component_has_tests.cache_clear()

        result = determine_jobs.detect_memory_impact_config()

    assert result["should_run"] == "true"
    assert result["components"] == ["api"]  # Fallback component
    assert result["platform"] == "esp32-idf"  # Fallback platform
    assert result["use_merged_config"] == "true"


def test_detect_memory_impact_config_core_python_only_changes(tmp_path: Path) -> None:
    """Test that Python-only core changes don't trigger memory impact analysis."""
    # Create test directory structure with fallback component
    tests_dir = tmp_path / "tests" / "components"

    # api component (fallback component) with esp32-idf test
    api_dir = tests_dir / "api"
    api_dir.mkdir(parents=True)
    (api_dir / "test.esp32-idf.yaml").write_text("test: api")

    # Mock changed_files to return only core Python files (no C++ files)
    with (
        patch.object(determine_jobs, "root_path", str(tmp_path)),
        patch.object(helpers, "root_path", str(tmp_path)),
        patch.object(determine_jobs, "changed_files") as mock_changed_files,
    ):
        mock_changed_files.return_value = [
            "esphome/__main__.py",
            "esphome/config.py",
            "esphome/core/config.py",
        ]
        determine_jobs._component_has_tests.cache_clear()

        result = determine_jobs.detect_memory_impact_config()

    # Python-only changes should NOT trigger memory impact analysis
    assert result["should_run"] == "false"


def test_detect_memory_impact_config_no_common_platform(tmp_path: Path) -> None:
    """Test memory impact detection when components have no common platform."""
    # Create test directory structure
    tests_dir = tmp_path / "tests" / "components"

    # wifi component only has esp32-idf test
    wifi_dir = tests_dir / "wifi"
    wifi_dir.mkdir(parents=True)
    (wifi_dir / "test.esp32-idf.yaml").write_text("test: wifi")

    # logger component only has esp8266-ard test
    logger_dir = tests_dir / "logger"
    logger_dir.mkdir(parents=True)
    (logger_dir / "test.esp8266-ard.yaml").write_text("test: logger")

    # Mock changed_files to return both components
    with (
        patch.object(determine_jobs, "root_path", str(tmp_path)),
        patch.object(helpers, "root_path", str(tmp_path)),
        patch.object(determine_jobs, "changed_files") as mock_changed_files,
    ):
        mock_changed_files.return_value = [
            "esphome/components/wifi/wifi.cpp",
            "esphome/components/logger/logger.cpp",
        ]
        determine_jobs._component_has_tests.cache_clear()

        result = determine_jobs.detect_memory_impact_config()

    # Should pick the most frequently supported platform
    assert result["should_run"] == "true"
    assert set(result["components"]) == {"wifi", "logger"}
    # When no common platform, picks most commonly supported
    # esp8266-ard is preferred over esp32-idf in the preference list
    assert result["platform"] in ["esp32-idf", "esp8266-ard"]
    assert result["use_merged_config"] == "true"


def test_detect_memory_impact_config_no_changes(tmp_path: Path) -> None:
    """Test memory impact detection when no files changed."""
    # Mock changed_files to return empty list
    with (
        patch.object(determine_jobs, "root_path", str(tmp_path)),
        patch.object(helpers, "root_path", str(tmp_path)),
        patch.object(determine_jobs, "changed_files") as mock_changed_files,
    ):
        mock_changed_files.return_value = []
        determine_jobs._component_has_tests.cache_clear()

        result = determine_jobs.detect_memory_impact_config()

    assert result["should_run"] == "false"


def test_detect_memory_impact_config_no_components_with_tests(tmp_path: Path) -> None:
    """Test memory impact detection when changed components have no tests."""
    # Create test directory structure
    tests_dir = tmp_path / "tests" / "components"

    # Create component directory but no test files
    custom_component_dir = tests_dir / "my_custom_component"
    custom_component_dir.mkdir(parents=True)

    # Mock changed_files to return component without tests
    with (
        patch.object(determine_jobs, "root_path", str(tmp_path)),
        patch.object(helpers, "root_path", str(tmp_path)),
        patch.object(determine_jobs, "changed_files") as mock_changed_files,
    ):
        mock_changed_files.return_value = [
            "esphome/components/my_custom_component/component.cpp",
        ]
        determine_jobs._component_has_tests.cache_clear()

        result = determine_jobs.detect_memory_impact_config()

    assert result["should_run"] == "false"


def test_detect_memory_impact_config_skips_base_bus_components(tmp_path: Path) -> None:
    """Test that base bus components (i2c, spi, uart) are skipped."""
    # Create test directory structure
    tests_dir = tmp_path / "tests" / "components"

    # i2c component (should be skipped as it's a base bus component)
    i2c_dir = tests_dir / "i2c"
    i2c_dir.mkdir(parents=True)
    (i2c_dir / "test.esp32-idf.yaml").write_text("test: i2c")

    # wifi component (should not be skipped)
    wifi_dir = tests_dir / "wifi"
    wifi_dir.mkdir(parents=True)
    (wifi_dir / "test.esp32-idf.yaml").write_text("test: wifi")

    # Mock changed_files to return both i2c and wifi
    with (
        patch.object(determine_jobs, "root_path", str(tmp_path)),
        patch.object(helpers, "root_path", str(tmp_path)),
        patch.object(determine_jobs, "changed_files") as mock_changed_files,
    ):
        mock_changed_files.return_value = [
            "esphome/components/i2c/i2c.cpp",
            "esphome/components/wifi/wifi.cpp",
        ]
        determine_jobs._component_has_tests.cache_clear()

        result = determine_jobs.detect_memory_impact_config()

    # Should only include wifi, not i2c
    assert result["should_run"] == "true"
    assert result["components"] == ["wifi"]
    assert "i2c" not in result["components"]


# Tests for clang-tidy split mode logic


def test_clang_tidy_mode_full_scan(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_changed_files: Mock,
    capsys: pytest.CaptureFixture[str],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Test that full scan (hash changed) always uses split mode."""
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)

    mock_should_run_integration_tests.return_value = False
    mock_should_run_clang_tidy.return_value = True
    mock_should_run_clang_format.return_value = False
    mock_should_run_python_linters.return_value = False

    # Mock changed_files to return no component files
    mock_changed_files.return_value = []

    # Mock full scan (hash changed)
    with (
        patch("sys.argv", ["determine-jobs.py"]),
        patch.object(determine_jobs, "_is_clang_tidy_full_scan", return_value=True),
        patch.object(determine_jobs, "get_changed_components", return_value=[]),
        patch.object(
            determine_jobs, "filter_component_and_test_files", return_value=False
        ),
        patch.object(
            determine_jobs, "get_components_with_dependencies", return_value=[]
        ),
    ):
        determine_jobs.main()

    captured = capsys.readouterr()
    output = json.loads(captured.out)

    # Full scan should always use split mode
    assert output["clang_tidy_mode"] == "split"


@pytest.mark.parametrize(
    ("component_count", "files_per_component", "expected_mode"),
    [
        # Small PR: 5 files in 1 component -> nosplit
        (1, 5, "nosplit"),
        # Medium PR: 30 files in 2 components -> nosplit
        (2, 15, "nosplit"),
        # Medium PR: 64 files total -> nosplit (just under threshold)
        (2, 32, "nosplit"),
        # Large PR: 65 files total -> split (at threshold)
        (2, 33, "split"),  # 2 * 33 = 66 files
        # Large PR: 100 files in 10 components -> split
        (10, 10, "split"),
    ],
    ids=[
        "1_comp_5_files_nosplit",
        "2_comp_30_files_nosplit",
        "2_comp_64_files_nosplit_under_threshold",
        "2_comp_66_files_split_at_threshold",
        "10_comp_100_files_split",
    ],
)
def test_clang_tidy_mode_targeted_scan(
    component_count: int,
    files_per_component: int,
    expected_mode: str,
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_changed_files: Mock,
    capsys: pytest.CaptureFixture[str],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Test clang-tidy mode selection based on files_to_check count."""
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)

    mock_should_run_integration_tests.return_value = False
    mock_should_run_clang_tidy.return_value = True
    mock_should_run_clang_format.return_value = False
    mock_should_run_python_linters.return_value = False

    # Create component names
    components = [f"comp{i}" for i in range(component_count)]

    # Mock changed_files to return component files
    mock_changed_files.return_value = [
        f"esphome/components/{comp}/file.cpp" for comp in components
    ]

    # Mock git_ls_files to return files for each component
    cpp_files = {
        f"esphome/components/{comp}/file{i}.cpp": 0
        for comp in components
        for i in range(files_per_component)
    }

    # Create a mock that returns the cpp_files dict for any call
    def mock_git_ls_files(patterns=None):
        return cpp_files

    with (
        patch("sys.argv", ["determine-jobs.py"]),
        patch.object(determine_jobs, "_is_clang_tidy_full_scan", return_value=False),
        patch.object(determine_jobs, "git_ls_files", side_effect=mock_git_ls_files),
        patch.object(determine_jobs, "get_changed_components", return_value=components),
        patch.object(
            determine_jobs,
            "filter_component_and_test_files",
            side_effect=lambda f: f.startswith("esphome/components/"),
        ),
        patch.object(
            determine_jobs, "get_components_with_dependencies", return_value=components
        ),
    ):
        determine_jobs.main()

    captured = capsys.readouterr()
    output = json.loads(captured.out)

    assert output["clang_tidy_mode"] == expected_mode


def test_main_core_files_changed_still_detects_components(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_changed_files: Mock,
    mock_determine_cpp_unit_tests: Mock,
    capsys: pytest.CaptureFixture[str],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Test that component changes are detected even when core files change."""
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)

    mock_should_run_integration_tests.return_value = True
    mock_should_run_clang_tidy.return_value = True
    mock_should_run_clang_format.return_value = True
    mock_should_run_python_linters.return_value = True
    mock_determine_cpp_unit_tests.return_value = (True, [])

    mock_changed_files.return_value = [
        "esphome/core/helpers.h",
        "esphome/components/select/select_traits.h",
        "esphome/components/select/select_traits.cpp",
        "esphome/components/api/api.proto",
    ]

    with (
        patch("sys.argv", ["determine-jobs.py"]),
        patch.object(determine_jobs, "_is_clang_tidy_full_scan", return_value=False),
        patch.object(determine_jobs, "get_changed_components", return_value=None),
        patch.object(
            determine_jobs,
            "filter_component_and_test_files",
            side_effect=lambda f: f.startswith("esphome/components/"),
        ),
        patch.object(
            determine_jobs,
            "get_components_with_dependencies",
            side_effect=lambda files, deps: (
                ["select", "api"]
                if not deps
                else ["select", "api", "bluetooth_proxy", "logger"]
            ),
        ),
    ):
        determine_jobs.main()

    captured = capsys.readouterr()
    output = json.loads(captured.out)

    assert output["clang_tidy"] is True
    assert output["clang_tidy_mode"] == "split"
    assert "select" in output["changed_components"]
    assert "api" in output["changed_components"]
    assert len(output["changed_components"]) > 0
