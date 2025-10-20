"""Unit tests for script/determine-jobs.py module."""

from collections.abc import Generator
import importlib.util
import json
import os
from pathlib import Path
import subprocess
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
def mock_subprocess_run() -> Generator[Mock, None, None]:
    """Mock subprocess.run for list-components.py calls."""
    with patch.object(determine_jobs.subprocess, "run") as mock:
        yield mock


@pytest.fixture
def mock_changed_files() -> Generator[Mock, None, None]:
    """Mock changed_files for memory impact detection."""
    with patch.object(determine_jobs, "changed_files") as mock:
        # Default to empty list
        mock.return_value = []
        yield mock


def test_main_all_tests_should_run(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_subprocess_run: Mock,
    mock_changed_files: Mock,
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

    # Mock list-components.py output (now returns JSON with --changed-with-deps)
    mock_result = Mock()
    mock_result.stdout = json.dumps(
        {"directly_changed": ["wifi", "api"], "all_changed": ["wifi", "api", "sensor"]}
    )
    mock_subprocess_run.return_value = mock_result

    # Run main function with mocked argv
    with patch("sys.argv", ["determine-jobs.py"]):
        determine_jobs.main()

    # Check output
    captured = capsys.readouterr()
    output = json.loads(captured.out)

    assert output["integration_tests"] is True
    assert output["clang_tidy"] is True
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
    # memory_impact should be present
    assert "memory_impact" in output
    assert output["memory_impact"]["should_run"] == "false"  # No files changed


def test_main_no_tests_should_run(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_subprocess_run: Mock,
    mock_changed_files: Mock,
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

    # Mock empty list-components.py output
    mock_result = Mock()
    mock_result.stdout = json.dumps({"directly_changed": [], "all_changed": []})
    mock_subprocess_run.return_value = mock_result

    # Run main function with mocked argv
    with patch("sys.argv", ["determine-jobs.py"]):
        determine_jobs.main()

    # Check output
    captured = capsys.readouterr()
    output = json.loads(captured.out)

    assert output["integration_tests"] is False
    assert output["clang_tidy"] is False
    assert output["clang_format"] is False
    assert output["python_linters"] is False
    assert output["changed_components"] == []
    assert output["changed_components_with_tests"] == []
    assert output["component_test_count"] == 0
    # memory_impact should be present
    assert "memory_impact" in output
    assert output["memory_impact"]["should_run"] == "false"


def test_main_list_components_fails(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_subprocess_run: Mock,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Test when list-components.py fails."""
    mock_should_run_integration_tests.return_value = True
    mock_should_run_clang_tidy.return_value = True
    mock_should_run_clang_format.return_value = True
    mock_should_run_python_linters.return_value = True

    # Mock list-components.py failure
    mock_subprocess_run.side_effect = subprocess.CalledProcessError(1, "cmd")

    # Run main function with mocked argv - should raise
    with (
        patch("sys.argv", ["determine-jobs.py"]),
        pytest.raises(subprocess.CalledProcessError),
    ):
        determine_jobs.main()


def test_main_with_branch_argument(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_subprocess_run: Mock,
    mock_changed_files: Mock,
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

    # Mock list-components.py output
    mock_result = Mock()
    mock_result.stdout = json.dumps(
        {"directly_changed": ["mqtt"], "all_changed": ["mqtt"]}
    )
    mock_subprocess_run.return_value = mock_result

    with patch("sys.argv", ["script.py", "-b", "main"]):
        determine_jobs.main()

    # Check that functions were called with branch
    mock_should_run_integration_tests.assert_called_once_with("main")
    mock_should_run_clang_tidy.assert_called_once_with("main")
    mock_should_run_clang_format.assert_called_once_with("main")
    mock_should_run_python_linters.assert_called_once_with("main")

    # Check that list-components.py was called with branch
    mock_subprocess_run.assert_called_once()
    call_args = mock_subprocess_run.call_args[0][0]
    assert "--changed-with-deps" in call_args
    assert "-b" in call_args
    assert "main" in call_args

    # Check output
    captured = capsys.readouterr()
    output = json.loads(captured.out)

    assert output["integration_tests"] is False
    assert output["clang_tidy"] is True
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
    # memory_impact should be present
    assert "memory_impact" in output
    assert output["memory_impact"]["should_run"] == "false"


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

    # Even with C++ files, exception should trigger clang-tidy
    with (
        patch.object(
            determine_jobs, "changed_files", return_value=["esphome/core.cpp"]
        ),
        patch("subprocess.run", side_effect=Exception("Hash check failed")),
    ):
        result = determine_jobs.should_run_clang_tidy()
        assert result is True


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


def test_main_filters_components_without_tests(
    mock_should_run_integration_tests: Mock,
    mock_should_run_clang_tidy: Mock,
    mock_should_run_clang_format: Mock,
    mock_should_run_python_linters: Mock,
    mock_subprocess_run: Mock,
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

    # Mock list-components.py output with 3 components
    # wifi: has tests, sensor: has tests, airthings_ble: no tests
    mock_result = Mock()
    mock_result.stdout = json.dumps(
        {
            "directly_changed": ["wifi", "sensor"],
            "all_changed": ["wifi", "sensor", "airthings_ble"],
        }
    )
    mock_subprocess_run.return_value = mock_result

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
