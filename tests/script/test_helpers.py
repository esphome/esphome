"""Unit tests for script/helpers.py module."""

import json
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import Mock, patch

import pytest
from pytest import MonkeyPatch

# Add the script directory to Python path so we can import helpers
sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "script"))
)

import helpers  # noqa: E402

changed_files = helpers.changed_files
filter_changed = helpers.filter_changed
get_changed_components = helpers.get_changed_components
_get_changed_files_from_command = helpers._get_changed_files_from_command
_get_pr_number_from_github_env = helpers._get_pr_number_from_github_env
_get_changed_files_github_actions = helpers._get_changed_files_github_actions
_filter_changed_ci = helpers._filter_changed_ci
_filter_changed_local = helpers._filter_changed_local
build_all_include = helpers.build_all_include
print_file_list = helpers.print_file_list
get_all_dependencies = helpers.get_all_dependencies


@pytest.mark.parametrize(
    ("github_ref", "expected_pr_number"),
    [
        ("refs/pull/1234/merge", "1234"),
        ("refs/pull/5678/head", "5678"),
        ("refs/pull/999/merge", "999"),
        ("refs/heads/main", None),
        ("", None),
    ],
)
def test_get_pr_number_from_github_env_ref(
    monkeypatch: MonkeyPatch, github_ref: str, expected_pr_number: str | None
) -> None:
    """Test extracting PR number from GITHUB_REF."""
    monkeypatch.setenv("GITHUB_REF", github_ref)
    # Make sure GITHUB_EVENT_PATH is not set
    monkeypatch.delenv("GITHUB_EVENT_PATH", raising=False)

    result = _get_pr_number_from_github_env()

    assert result == expected_pr_number


def test_get_pr_number_from_github_env_event_file(
    monkeypatch: MonkeyPatch, tmp_path: Path
) -> None:
    """Test extracting PR number from GitHub event file."""
    # No PR number in ref
    monkeypatch.setenv("GITHUB_REF", "refs/heads/feature-branch")

    event_file = tmp_path / "event.json"
    event_data = {"pull_request": {"number": 5678}}
    event_file.write_text(json.dumps(event_data))
    monkeypatch.setenv("GITHUB_EVENT_PATH", str(event_file))

    result = _get_pr_number_from_github_env()

    assert result == "5678"


def test_get_pr_number_from_github_env_no_pr(
    monkeypatch: MonkeyPatch, tmp_path: Path
) -> None:
    """Test when no PR number is available."""
    monkeypatch.setenv("GITHUB_REF", "refs/heads/main")

    event_file = tmp_path / "event.json"
    event_data = {"push": {"head_commit": {"id": "abc123"}}}
    event_file.write_text(json.dumps(event_data))
    monkeypatch.setenv("GITHUB_EVENT_PATH", str(event_file))

    result = _get_pr_number_from_github_env()

    assert result is None


@pytest.mark.parametrize(
    ("github_ref", "expected_pr_number"),
    [
        ("refs/pull/1234/merge", "1234"),
        ("refs/pull/5678/head", "5678"),
        ("refs/pull/999/merge", "999"),
    ],
)
def test_github_actions_pull_request_with_pr_number_in_ref(
    monkeypatch: MonkeyPatch, github_ref: str, expected_pr_number: str
) -> None:
    """Test PR detection via GITHUB_REF."""
    monkeypatch.setenv("GITHUB_ACTIONS", "true")
    monkeypatch.setenv("GITHUB_EVENT_NAME", "pull_request")
    monkeypatch.setenv("GITHUB_REF", github_ref)

    expected_files = ["file1.py", "file2.cpp"]

    with patch("helpers._get_changed_files_from_command") as mock_get:
        mock_get.return_value = expected_files

        result = changed_files()

        mock_get.assert_called_once_with(
            ["gh", "pr", "diff", expected_pr_number, "--name-only"]
        )
        assert result == expected_files


def test_github_actions_pull_request_with_event_file(
    monkeypatch: MonkeyPatch, tmp_path: Path
) -> None:
    """Test PR detection via GitHub event file."""
    monkeypatch.setenv("GITHUB_ACTIONS", "true")
    monkeypatch.setenv("GITHUB_EVENT_NAME", "pull_request")
    monkeypatch.setenv("GITHUB_REF", "refs/heads/feature-branch")

    event_file = tmp_path / "event.json"
    event_data = {"pull_request": {"number": 5678}}
    event_file.write_text(json.dumps(event_data))
    monkeypatch.setenv("GITHUB_EVENT_PATH", str(event_file))

    expected_files = ["file1.py", "file2.cpp"]

    with patch("helpers._get_changed_files_from_command") as mock_get:
        mock_get.return_value = expected_files

        result = changed_files()

        mock_get.assert_called_once_with(["gh", "pr", "diff", "5678", "--name-only"])
        assert result == expected_files


def test_github_actions_push_event(monkeypatch: MonkeyPatch) -> None:
    """Test push event handling."""
    monkeypatch.setenv("GITHUB_ACTIONS", "true")
    monkeypatch.setenv("GITHUB_EVENT_NAME", "push")

    expected_files = ["file1.py", "file2.cpp"]

    with patch("helpers._get_changed_files_from_command") as mock_get:
        mock_get.return_value = expected_files

        result = changed_files()

        mock_get.assert_called_once_with(["git", "diff", "HEAD~1..HEAD", "--name-only"])
        assert result == expected_files


@pytest.fixture(autouse=True)
def clear_caches():
    """Clear function caches before each test."""
    # Clear the cache for _get_changed_files_github_actions
    _get_changed_files_github_actions.cache_clear()
    yield


def test_get_changed_files_github_actions_pull_request(
    monkeypatch: MonkeyPatch,
) -> None:
    """Test _get_changed_files_github_actions for pull request event."""
    monkeypatch.setenv("GITHUB_EVENT_NAME", "pull_request")

    expected_files = ["file1.py", "file2.cpp"]

    with (
        patch("helpers._get_pr_number_from_github_env", return_value="1234"),
        patch("helpers._get_changed_files_from_command") as mock_get,
    ):
        mock_get.return_value = expected_files

        result = _get_changed_files_github_actions()

        mock_get.assert_called_once_with(["gh", "pr", "diff", "1234", "--name-only"])
        assert result == expected_files


def test_get_changed_files_github_actions_pull_request_large_pr(
    monkeypatch: MonkeyPatch,
) -> None:
    """Test _get_changed_files_github_actions fallback for PRs with >300 files."""
    monkeypatch.setenv("GITHUB_EVENT_NAME", "pull_request")

    expected_files = ["file1.py", "file2.cpp"]

    with (
        patch("helpers._get_pr_number_from_github_env", return_value="10214"),
        patch("helpers._get_changed_files_from_command") as mock_get,
    ):
        # First call fails with too many files error, second succeeds with API method
        mock_get.side_effect = [
            Exception("Sorry, the diff exceeded the maximum number of files (300)"),
            expected_files,
        ]

        result = _get_changed_files_github_actions()

        assert mock_get.call_count == 2
        mock_get.assert_any_call(["gh", "pr", "diff", "10214", "--name-only"])
        mock_get.assert_any_call(
            [
                "gh",
                "api",
                "repos/esphome/esphome/pulls/10214/files",
                "--paginate",
                "--jq",
                ".[].filename",
            ]
        )
        assert result == expected_files


def test_get_changed_files_github_actions_pull_request_other_error(
    monkeypatch: MonkeyPatch,
) -> None:
    """Test _get_changed_files_github_actions re-raises non-file-limit errors."""
    monkeypatch.setenv("GITHUB_EVENT_NAME", "pull_request")

    with (
        patch("helpers._get_pr_number_from_github_env", return_value="1234"),
        patch("helpers._get_changed_files_from_command") as mock_get,
    ):
        # Error that is not about file limit
        mock_get.side_effect = Exception("Command failed: authentication required")

        with pytest.raises(Exception, match="authentication required"):
            _get_changed_files_github_actions()

        # Should only be called once (no retry with API)
        mock_get.assert_called_once_with(["gh", "pr", "diff", "1234", "--name-only"])


def test_get_changed_files_github_actions_pull_request_no_pr_number(
    monkeypatch: MonkeyPatch,
) -> None:
    """Test _get_changed_files_github_actions when no PR number is found."""
    monkeypatch.setenv("GITHUB_EVENT_NAME", "pull_request")

    with patch("helpers._get_pr_number_from_github_env", return_value=None):
        result = _get_changed_files_github_actions()

        assert result is None


def test_get_changed_files_github_actions_push(monkeypatch: MonkeyPatch) -> None:
    """Test _get_changed_files_github_actions for push event."""
    monkeypatch.setenv("GITHUB_EVENT_NAME", "push")

    expected_files = ["file1.py", "file2.cpp"]

    with patch("helpers._get_changed_files_from_command") as mock_get:
        mock_get.return_value = expected_files

        result = _get_changed_files_github_actions()

        mock_get.assert_called_once_with(["git", "diff", "HEAD~1..HEAD", "--name-only"])
        assert result == expected_files


def test_get_changed_files_github_actions_push_fallback(
    monkeypatch: MonkeyPatch,
) -> None:
    """Test _get_changed_files_github_actions fallback for push event."""
    monkeypatch.setenv("GITHUB_EVENT_NAME", "push")

    with patch("helpers._get_changed_files_from_command") as mock_get:
        mock_get.side_effect = Exception("Failed")

        result = _get_changed_files_github_actions()

        assert result is None


def test_get_changed_files_github_actions_other_event(monkeypatch: MonkeyPatch) -> None:
    """Test _get_changed_files_github_actions for other event types."""
    monkeypatch.setenv("GITHUB_EVENT_NAME", "workflow_dispatch")

    result = _get_changed_files_github_actions()

    assert result is None


def test_github_actions_push_event_fallback(monkeypatch: MonkeyPatch) -> None:
    """Test push event fallback to git merge-base."""
    monkeypatch.setenv("GITHUB_ACTIONS", "true")
    monkeypatch.setenv("GITHUB_EVENT_NAME", "push")

    expected_files = ["file1.py", "file2.cpp"]

    with (
        patch("helpers._get_changed_files_from_command") as mock_get,
        patch("helpers.get_output") as mock_output,
    ):
        # First call fails, triggering fallback
        mock_get.side_effect = [
            Exception("Failed"),
            expected_files,
        ]

        mock_output.side_effect = [
            "origin\nupstream\n",  # git remote
            "abc123\n",  # merge base
        ]

        result = changed_files()

        assert mock_get.call_count == 2
        assert result == expected_files


@pytest.mark.parametrize(
    ("branch", "merge_base"),
    [
        (None, "abc123"),  # Default branch (dev)
        ("release", "def456"),
        ("beta", "ghi789"),
    ],
)
def test_local_development_branches(
    monkeypatch: MonkeyPatch, branch: str | None, merge_base: str
) -> None:
    """Test local development with different branches."""
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)

    expected_files = ["file1.py", "file2.cpp"]

    with (
        patch("helpers.get_output") as mock_output,
        patch("helpers._get_changed_files_from_command") as mock_get,
    ):
        if branch is None:
            # For default branch, helpers.get_output is called twice (git remote and merge-base)
            mock_output.side_effect = [
                "origin\nupstream\n",  # git remote
                f"{merge_base}\n",  # merge base for upstream/dev
            ]
        else:
            # For custom branch, may need more calls if trying multiple remotes
            mock_output.side_effect = [
                "origin\nupstream\n",  # git remote
                Exception("not found"),  # upstream/{branch} may fail
                f"{merge_base}\n",  # merge base for origin/{branch}
            ]

        mock_get.return_value = expected_files

        result = changed_files(branch)

        mock_get.assert_called_once_with(["git", "diff", merge_base, "--name-only"])
        assert result == expected_files


def test_local_development_no_remotes_configured(monkeypatch: MonkeyPatch) -> None:
    """Test error when no git remotes are configured."""
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)

    with patch("helpers.get_output") as mock_output:
        # The function calls get_output multiple times:
        # 1. First to get list of remotes: git remote
        # 2. Then for each remote it tries: git merge-base
        # We simulate having some remotes but all merge-base attempts fail
        def side_effect_func(*args):
            if args == ("git", "remote"):
                return "origin\nupstream\n"
            # All merge-base attempts fail
            raise Exception("Command failed")

        mock_output.side_effect = side_effect_func

        with pytest.raises(ValueError, match="Git not configured"):
            changed_files()


@pytest.mark.parametrize(
    ("stdout", "expected"),
    [
        ("file1.py\nfile2.cpp\n\n", ["file1.py", "file2.cpp"]),
        ("\n\n", []),
        ("single.py\n", ["single.py"]),
        (
            "path/to/file.cpp\nanother/path.h\n",
            ["another/path.h", "path/to/file.cpp"],
        ),  # Sorted
    ],
)
def test_get_changed_files_from_command_successful(
    stdout: str, expected: list[str]
) -> None:
    """Test successful command execution with various outputs."""
    mock_result = Mock()
    mock_result.returncode = 0
    mock_result.stdout = stdout

    with patch("subprocess.run", return_value=mock_result):
        result = _get_changed_files_from_command(["git", "diff"])

        # Normalize paths to forward slashes for comparison
        # since os.path.relpath returns OS-specific separators
        normalized_result = [f.replace(os.sep, "/") for f in result]
        assert normalized_result == expected


@pytest.mark.parametrize(
    ("returncode", "stderr"),
    [
        (1, "Error: command failed"),
        (128, "fatal: not a git repository"),
        (2, "Unknown error"),
    ],
)
def test_get_changed_files_from_command_failed(returncode: int, stderr: str) -> None:
    """Test command failure handling."""
    mock_result = Mock()
    mock_result.returncode = returncode
    mock_result.stderr = stderr

    with patch("subprocess.run", return_value=mock_result):
        with pytest.raises(Exception) as exc_info:
            _get_changed_files_from_command(["git", "diff"])
        assert "Command failed" in str(exc_info.value)
        assert stderr in str(exc_info.value)


def test_get_changed_files_from_command_relative_paths() -> None:
    """Test that paths are made relative to current directory."""
    mock_result = Mock()
    mock_result.returncode = 0
    mock_result.stdout = "/some/project/file1.py\n/some/project/sub/file2.cpp\n"

    with (
        patch("subprocess.run", return_value=mock_result),
        patch(
            "os.path.relpath", side_effect=["file1.py", "sub/file2.cpp"]
        ) as mock_relpath,
        patch("os.getcwd", return_value="/some/project"),
    ):
        result = _get_changed_files_from_command(["git", "diff"])

        # Check relpath was called with correct arguments
        assert mock_relpath.call_count == 2
        assert result == ["file1.py", "sub/file2.cpp"]


@pytest.mark.parametrize(
    "changed_files_list",
    [
        ["esphome/core/component.h", "esphome/components/wifi/wifi.cpp"],
        ["esphome/core/helpers.cpp"],
        ["esphome/core/application.h", "esphome/core/defines.h"],
    ],
)
def test_get_changed_components_core_cpp_files_trigger_full_scan(
    changed_files_list: list[str],
) -> None:
    """Test that core C++/header file changes trigger full scan without calling subprocess."""
    with patch("helpers.changed_files") as mock_changed:
        mock_changed.return_value = changed_files_list

        # Should return None without calling subprocess
        result = get_changed_components()
        assert result is None


def test_get_changed_components_core_python_files_no_full_scan() -> None:
    """Test that core Python file changes do NOT trigger full scan."""
    changed_files_list = [
        "esphome/core/__init__.py",
        "esphome/core/config.py",
        "esphome/components/wifi/wifi.cpp",
    ]

    with patch("helpers.changed_files") as mock_changed:
        mock_changed.return_value = changed_files_list

        mock_result = Mock()
        mock_result.stdout = "wifi\n"

        with patch("subprocess.run", return_value=mock_result):
            result = get_changed_components()
            # Should NOT return None - should call list-components.py
            assert result == ["wifi"]


def test_get_changed_components_mixed_core_files_with_cpp() -> None:
    """Test that mixed Python and C++ core files still trigger full scan due to C++ file."""
    changed_files_list = [
        "esphome/core/__init__.py",
        "esphome/core/config.py",
        "esphome/core/helpers.cpp",  # This C++ file should trigger full scan
        "esphome/components/wifi/wifi.cpp",
    ]

    with patch("helpers.changed_files") as mock_changed:
        mock_changed.return_value = changed_files_list

        # Should return None without calling subprocess due to helpers.cpp
        result = get_changed_components()
        assert result is None


@pytest.mark.parametrize(
    ("changed_files_list", "expected"),
    [
        # Only component files changed
        (
            ["esphome/components/wifi/wifi.cpp", "esphome/components/api/api.cpp"],
            ["wifi", "api"],
        ),
        # Non-component files only
        (["README.md", "script/clang-tidy"], []),
        # Single component
        (["esphome/components/mqtt/mqtt_client.cpp"], ["mqtt"]),
    ],
)
def test_get_changed_components_returns_component_list(
    changed_files_list: list[str], expected: list[str]
) -> None:
    """Test component detection returns correct component list."""
    with patch("helpers.changed_files") as mock_changed:
        mock_changed.return_value = changed_files_list

        mock_result = Mock()
        mock_result.stdout = "\n".join(expected) + "\n" if expected else "\n"

        with patch("subprocess.run", return_value=mock_result):
            result = get_changed_components()
            assert result == expected


def test_get_changed_components_script_failure() -> None:
    """Test fallback to full scan when script fails."""
    with patch("helpers.changed_files") as mock_changed:
        mock_changed.return_value = ["esphome/components/wifi/wifi_component.cpp"]

        with patch("subprocess.run") as mock_run:
            mock_run.side_effect = subprocess.CalledProcessError(1, "cmd")

            result = get_changed_components()

            assert result is None  # None means full scan


@pytest.mark.parametrize(
    ("components", "all_files", "expected_files"),
    [
        # Core C++/header files changed (full scan)
        (
            None,
            ["esphome/components/wifi/wifi.cpp", "esphome/core/helpers.cpp"],
            ["esphome/components/wifi/wifi.cpp", "esphome/core/helpers.cpp"],
        ),
        # Specific components
        (
            ["wifi", "api"],
            [
                "esphome/components/wifi/wifi.cpp",
                "esphome/components/wifi/wifi.h",
                "esphome/components/api/api.cpp",
                "esphome/components/mqtt/mqtt.cpp",
            ],
            [
                "esphome/components/wifi/wifi.cpp",
                "esphome/components/wifi/wifi.h",
                "esphome/components/api/api.cpp",
            ],
        ),
        # No components changed
        (
            [],
            ["esphome/components/wifi/wifi.cpp", "script/clang-tidy"],
            ["script/clang-tidy"],  # Only non-component changed files
        ),
    ],
)
def test_filter_changed_ci_mode(
    monkeypatch: MonkeyPatch,
    components: list[str] | None,
    all_files: list[str],
    expected_files: list[str],
) -> None:
    """Test filter_changed in CI mode with different component scenarios."""
    monkeypatch.setenv("GITHUB_ACTIONS", "true")

    with patch("helpers.get_changed_components") as mock_components:
        mock_components.return_value = components

        if components == []:
            # No components changed scenario needs changed_files mock
            with patch("helpers.changed_files") as mock_changed:
                mock_changed.return_value = ["script/clang-tidy", "README.md"]
                result = filter_changed(all_files)
        else:
            result = filter_changed(all_files)

        assert set(result) == set(expected_files)


def test_filter_changed_local_mode(monkeypatch: MonkeyPatch) -> None:
    """Test filter_changed in local mode filters files directly."""
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)

    all_files = [
        "esphome/components/wifi/wifi.cpp",
        "esphome/components/api/api.cpp",
        "esphome/core/helpers.cpp",
    ]

    with patch("helpers.changed_files") as mock_changed:
        mock_changed.return_value = [
            "esphome/components/wifi/wifi.cpp",
            "esphome/core/helpers.cpp",
        ]

        result = filter_changed(all_files)

        # Should only include files that actually changed
        expected = ["esphome/components/wifi/wifi.cpp", "esphome/core/helpers.cpp"]
        assert set(result) == set(expected)


def test_filter_changed_component_path_parsing(monkeypatch: MonkeyPatch) -> None:
    """Test correct parsing of component paths."""
    monkeypatch.setenv("GITHUB_ACTIONS", "true")

    all_files = [
        "esphome/components/wifi/wifi_component.cpp",
        "esphome/components/wifi_info/wifi_info_text_sensor.cpp",  # Different component
        "esphome/components/api/api_server.cpp",
        "esphome/components/api/custom_api_device.h",
    ]

    with patch("helpers.get_changed_components") as mock_components:
        mock_components.return_value = ["wifi"]  # Only wifi, not wifi_info

        result = filter_changed(all_files)

        # Should only include files from wifi component, not wifi_info
        expected = ["esphome/components/wifi/wifi_component.cpp"]
        assert result == expected


def test_filter_changed_prints_output(
    monkeypatch: MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that appropriate messages are printed."""
    monkeypatch.setenv("GITHUB_ACTIONS", "true")

    all_files = ["esphome/components/wifi/wifi_component.cpp"]

    with patch("helpers.get_changed_components") as mock_components:
        mock_components.return_value = ["wifi"]

        filter_changed(all_files)

        # Check that output was produced (not checking exact messages)
        captured = capsys.readouterr()
        assert len(captured.out) > 0


@pytest.mark.parametrize(
    ("files", "expected_empty"),
    [
        ([], True),
        (["file.cpp"], False),
    ],
    ids=["empty_files", "non_empty_files"],
)
def test_filter_changed_empty_file_handling(
    monkeypatch: MonkeyPatch, files: list[str], expected_empty: bool
) -> None:
    """Test handling of empty file lists."""
    monkeypatch.setenv("GITHUB_ACTIONS", "true")

    with patch("helpers.get_changed_components") as mock_components:
        mock_components.return_value = ["wifi"]

        result = filter_changed(files)

        # Both cases should be empty:
        # - Empty files list -> empty result
        # - file.cpp doesn't match esphome/components/wifi/* pattern -> filtered out
        assert len(result) == 0


def test_filter_changed_ci_full_scan() -> None:
    """Test _filter_changed_ci when core C++/header files changed (full scan)."""
    all_files = ["esphome/components/wifi/wifi.cpp", "esphome/core/helpers.cpp"]

    with patch("helpers.get_changed_components", return_value=None):
        result = _filter_changed_ci(all_files)

    # Should return all files for full scan
    assert result == all_files


def test_filter_changed_ci_no_components_changed() -> None:
    """Test _filter_changed_ci when no components changed."""
    all_files = ["esphome/components/wifi/wifi.cpp", "script/clang-tidy", "README.md"]

    with (
        patch("helpers.get_changed_components", return_value=[]),
        patch("helpers.changed_files", return_value=["script/clang-tidy", "README.md"]),
    ):
        result = _filter_changed_ci(all_files)

    # Should only include non-component files that changed
    assert set(result) == {"script/clang-tidy", "README.md"}


def test_filter_changed_ci_specific_components() -> None:
    """Test _filter_changed_ci with specific components changed."""
    all_files = [
        "esphome/components/wifi/wifi.cpp",
        "esphome/components/wifi/wifi.h",
        "esphome/components/api/api.cpp",
        "esphome/components/mqtt/mqtt.cpp",
    ]

    with patch("helpers.get_changed_components", return_value=["wifi", "api"]):
        result = _filter_changed_ci(all_files)

    # Should include all files from wifi and api components
    expected = [
        "esphome/components/wifi/wifi.cpp",
        "esphome/components/wifi/wifi.h",
        "esphome/components/api/api.cpp",
    ]
    assert set(result) == set(expected)


def test_filter_changed_local() -> None:
    """Test _filter_changed_local filters based on git changes."""
    all_files = [
        "esphome/components/wifi/wifi.cpp",
        "esphome/components/api/api.cpp",
        "esphome/core/helpers.cpp",
    ]

    with patch("helpers.changed_files") as mock_changed:
        mock_changed.return_value = [
            "esphome/components/wifi/wifi.cpp",
            "esphome/core/helpers.cpp",
        ]

        result = _filter_changed_local(all_files)

    # Should only include files that actually changed
    expected = ["esphome/components/wifi/wifi.cpp", "esphome/core/helpers.cpp"]
    assert set(result) == set(expected)


def test_build_all_include_with_git(tmp_path: Path) -> None:
    """Test build_all_include using git ls-files."""
    # Mock git output
    git_output = "esphome/core/component.h\nesphome/components/wifi/wifi.h\nesphome/components/api/api.h\n"

    mock_proc = Mock()
    mock_proc.returncode = 0
    mock_proc.stdout = git_output

    with (
        patch("subprocess.run", return_value=mock_proc),
        patch("helpers.temp_header_file", str(tmp_path / "all-include.cpp")),
    ):
        build_all_include()

    # Check the generated file
    include_file = tmp_path / "all-include.cpp"
    assert include_file.exists()

    content = include_file.read_text()
    expected_lines = [
        '#include "esphome/components/api/api.h"',
        '#include "esphome/components/wifi/wifi.h"',
        '#include "esphome/core/component.h"',
        "",  # Empty line at end
    ]
    assert content == "\n".join(expected_lines)


def test_build_all_include_empty_output(tmp_path: Path) -> None:
    """Test build_all_include with empty git output."""
    # Mock git returning empty output
    mock_proc = Mock()
    mock_proc.returncode = 0
    mock_proc.stdout = ""

    with (
        patch("subprocess.run", return_value=mock_proc),
        patch("helpers.temp_header_file", str(tmp_path / "all-include.cpp")),
    ):
        build_all_include()

    # Check the generated file
    include_file = tmp_path / "all-include.cpp"
    assert include_file.exists()

    content = include_file.read_text()
    # When git output is empty, the list comprehension filters out empty strings,
    # then we append "" to get [""], which joins to just ""
    assert content == ""


def test_build_all_include_creates_directory(tmp_path: Path) -> None:
    """Test that build_all_include creates the temp directory if needed."""
    # Use a subdirectory that doesn't exist
    temp_file = tmp_path / "subdir" / "all-include.cpp"

    mock_proc = Mock()
    mock_proc.returncode = 0
    mock_proc.stdout = "esphome/core/test.h\n"

    with (
        patch("subprocess.run", return_value=mock_proc),
        patch("helpers.temp_header_file", str(temp_file)),
    ):
        build_all_include()

    # Check that directory was created
    assert temp_file.parent.exists()
    assert temp_file.exists()


def test_print_file_list_empty(capsys: pytest.CaptureFixture[str]) -> None:
    """Test printing an empty file list."""
    print_file_list([], "Test Files:")
    captured = capsys.readouterr()

    assert "Test Files:" in captured.out
    assert "No files to check!" in captured.out


def test_print_file_list_small(capsys: pytest.CaptureFixture[str]) -> None:
    """Test printing a small list of files (less than max_files)."""
    files = ["file1.cpp", "file2.cpp", "file3.cpp"]
    print_file_list(files, "Test Files:", max_files=20)
    captured = capsys.readouterr()

    assert "Test Files:" in captured.out
    assert "    file1.cpp" in captured.out
    assert "    file2.cpp" in captured.out
    assert "    file3.cpp" in captured.out
    assert "... and" not in captured.out


def test_print_file_list_exact_max_files(capsys: pytest.CaptureFixture[str]) -> None:
    """Test printing exactly max_files number of files."""
    files = [f"file{i}.cpp" for i in range(20)]
    print_file_list(files, "Test Files:", max_files=20)
    captured = capsys.readouterr()

    # All files should be shown
    for i in range(20):
        assert f"    file{i}.cpp" in captured.out
    assert "... and" not in captured.out


def test_print_file_list_large(capsys: pytest.CaptureFixture[str]) -> None:
    """Test printing a large list of files (more than max_files)."""
    files = [f"file{i:03d}.cpp" for i in range(50)]
    print_file_list(files, "Test Files:", max_files=20)
    captured = capsys.readouterr()

    assert "Test Files:" in captured.out
    # First 10 files should be shown (sorted)
    for i in range(10):
        assert f"    file{i:03d}.cpp" in captured.out
    # Files 10-49 should not be shown
    assert "    file010.cpp" not in captured.out
    assert "    file049.cpp" not in captured.out
    # Should show count of remaining files
    assert "... and 40 more files" in captured.out


def test_print_file_list_unsorted(capsys: pytest.CaptureFixture[str]) -> None:
    """Test that files are sorted before printing."""
    files = ["z_file.cpp", "a_file.cpp", "m_file.cpp"]
    print_file_list(files, "Test Files:", max_files=20)
    captured = capsys.readouterr()

    lines = captured.out.strip().split("\n")
    # Check order in output
    assert lines[1] == "    a_file.cpp"
    assert lines[2] == "    m_file.cpp"
    assert lines[3] == "    z_file.cpp"


def test_print_file_list_custom_max_files(capsys: pytest.CaptureFixture[str]) -> None:
    """Test with custom max_files parameter."""
    files = [f"file{i}.cpp" for i in range(15)]
    print_file_list(files, "Test Files:", max_files=10)
    captured = capsys.readouterr()

    # Should truncate after 10 files
    assert "... and 5 more files" in captured.out


def test_print_file_list_default_title(capsys: pytest.CaptureFixture[str]) -> None:
    """Test with default title."""
    print_file_list(["test.cpp"])
    captured = capsys.readouterr()

    assert "Files:" in captured.out
    assert "    test.cpp" in captured.out


@pytest.mark.parametrize(
    ("component_configs", "initial_components", "expected_components"),
    [
        # No dependencies
        (
            {"sensor": ([], [])},  # (dependencies, auto_load)
            {"sensor"},
            {"sensor"},
        ),
        # Simple dependencies
        (
            {
                "sensor": (["esp32"], []),
                "esp32": ([], []),
            },
            {"sensor"},
            {"sensor", "esp32"},
        ),
        # Auto-load components
        (
            {
                "light": ([], ["output", "power_supply"]),
                "output": ([], []),
                "power_supply": ([], []),
            },
            {"light"},
            {"light", "output", "power_supply"},
        ),
        # Transitive dependencies
        (
            {
                "comp_a": (["comp_b"], []),
                "comp_b": (["comp_c"], []),
                "comp_c": ([], []),
            },
            {"comp_a"},
            {"comp_a", "comp_b", "comp_c"},
        ),
        # Dependencies with dots (sensor.base)
        (
            {
                "my_comp": (["sensor.base", "binary_sensor.base"], []),
                "sensor": ([], []),
                "binary_sensor": ([], []),
            },
            {"my_comp"},
            {"my_comp", "sensor", "binary_sensor"},
        ),
        # Circular dependencies (should not cause infinite loop)
        (
            {
                "comp_a": (["comp_b"], []),
                "comp_b": (["comp_a"], []),
            },
            {"comp_a"},
            {"comp_a", "comp_b"},
        ),
    ],
)
def test_get_all_dependencies(
    component_configs: dict[str, tuple[list[str], list[str]]],
    initial_components: set[str],
    expected_components: set[str],
) -> None:
    """Test dependency resolution for components."""
    with patch("esphome.loader.get_component") as mock_get_component:

        def get_component_side_effect(name: str):
            if name in component_configs:
                deps, auto_load = component_configs[name]
                comp = Mock()
                comp.dependencies = deps
                comp.auto_load = auto_load
                return comp
            return None

        mock_get_component.side_effect = get_component_side_effect

        result = helpers.get_all_dependencies(initial_components)

        assert result == expected_components


def test_get_all_dependencies_handles_missing_components() -> None:
    """Test handling of components that can't be loaded."""
    with patch("esphome.loader.get_component") as mock_get_component:
        # First component exists, its dependency doesn't
        comp = Mock()
        comp.dependencies = ["missing_comp"]
        comp.auto_load = []

        mock_get_component.side_effect = (
            lambda name: comp if name == "existing" else None
        )

        result = helpers.get_all_dependencies({"existing", "nonexistent"})

        # Should still include all components, even if some can't be loaded
        assert result == {"existing", "nonexistent", "missing_comp"}


def test_get_all_dependencies_empty_set() -> None:
    """Test with empty initial component set."""
    result = helpers.get_all_dependencies(set())
    assert result == set()


def test_get_components_from_integration_fixtures() -> None:
    """Test extraction of components from fixture YAML files."""
    yaml_content = {
        "sensor": [{"platform": "template", "name": "test"}],
        "binary_sensor": [{"platform": "gpio", "pin": 5}],
        "esphome": {"name": "test"},
        "api": {},
    }
    expected_components = {
        "sensor",
        "binary_sensor",
        "esphome",
        "api",
        "template",
        "gpio",
    }

    mock_yaml_file = Mock()

    with (
        patch("pathlib.Path.glob") as mock_glob,
        patch("esphome.yaml_util.load_yaml", return_value=yaml_content),
    ):
        mock_glob.return_value = [mock_yaml_file]

        components = helpers.get_components_from_integration_fixtures()

        assert components == expected_components


@pytest.mark.parametrize(
    "output,expected",
    [
        ("wifi\napi\nsensor\n", ["wifi", "api", "sensor"]),
        ("wifi\n", ["wifi"]),
        ("", []),
        ("  \n  \n", []),
        ("\n\n", []),
        ("  wifi  \n  api  \n", ["wifi", "api"]),
        ("wifi\n\napi\n\nsensor", ["wifi", "api", "sensor"]),
    ],
)
def test_parse_list_components_output(output: str, expected: list[str]) -> None:
    """Test parse_list_components_output function."""
    result = helpers.parse_list_components_output(output)
    assert result == expected
