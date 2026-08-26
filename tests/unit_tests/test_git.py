"""Tests for git.py module."""

from collections.abc import Callable
import errno
import logging
import os
from pathlib import Path
import subprocess
import threading
import time
from typing import Any
from unittest.mock import Mock, patch

from filelock import FileLock
import pytest

from esphome import git
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError, TimePeriodSeconds
from esphome.git import GitCommandError


def _compute_repo_dir(url: str, ref: str | None, domain: str) -> Path:
    """Helper to compute the expected repo directory path using git module's logic."""
    key = f"{url}@{ref}"
    return git._compute_destination_path(key, domain)


# The tests must probe the exact location the implementation uses
_marker_path = git._clone_complete_marker_path


def _mark_clone_complete(repo_dir: Path) -> None:
    """Write the completion marker so a hand-made repo dir is treated as valid."""
    _marker_path(repo_dir).write_text("test")


def _setup_old_repo(repo_dir: Path, days_old: int = 2) -> None:
    """Helper to set up a git repo directory structure with an old timestamp.

    Args:
        repo_dir: The repository directory path to create.
        days_old: Number of days old to make the FETCH_HEAD file (default: 2).
    """
    # Create repo directory
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with old timestamp
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    old_time = time.time() - days_old * 86400
    fetch_head.touch()
    os.utime(fetch_head, (old_time, old_time))


def _get_git_command_type(cmd: list[str]) -> str | None:
    """Helper to determine the type of git command from a command list.

    Args:
        cmd: The git command list (e.g., ["git", "rev-parse", "HEAD"]).

    Returns:
        The command type ("rev-parse", "stash", "fetch", "reset", "clone") or None.
    """
    # Git commands are always in format ["git", "command", ...], so check index 1
    if len(cmd) > 1:
        return cmd[1]
    return None


def _simulate_cloned_repo(repo_dir: Path) -> None:
    """Create the directory structure a successful git clone would leave."""
    repo_dir.mkdir(parents=True, exist_ok=True)
    (repo_dir / ".git").mkdir(exist_ok=True)


def _make_clone_side_effect(
    repo_dir: Path,
    gitmodules: bool = False,
    on_clone: Callable[[], None] | None = None,
) -> Callable[..., str]:
    """Return a run_git_command side effect whose clone creates the repo dir.

    With ``gitmodules`` the cloned repo also declares submodules. ``on_clone``
    runs at clone time before the repo dir appears, so a test can probe or
    block mid-clone.
    """

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "clone":
            if on_clone is not None:
                on_clone()
            _simulate_cloned_repo(repo_dir)
            if gitmodules:
                (repo_dir / ".gitmodules").write_text("test")
        return ""

    return git_command_side_effect


def _submodule_calls(mock: Mock) -> list[Any]:
    """Return the mock's `git submodule` calls."""
    return [
        c for c in mock.call_args_list if _get_git_command_type(c[0][0]) == "submodule"
    ]


def _assert_submodule_runs_without_isolation(call: Any, repo_dir: Path) -> None:
    """Assert a git submodule call ran with plain cwd, not GIT_DIR/GIT_WORK_TREE
    isolation, which breaks the submodule porcelain on some installations."""
    assert call.kwargs.get("git_dir") is None
    assert call.kwargs.get("cwd") == repo_dir


def test_run_git_command_success(tmp_path: Path) -> None:
    """Test that run_git_command returns output on success."""
    # Create a simple git repo to test with
    repo_dir = tmp_path / "test_repo"
    repo_dir.mkdir()

    # Initialize a git repo
    result = git.run_git_command(["git", "init"], str(repo_dir))
    assert "Initialized empty Git repository" in result or result == ""

    # Verify we can run a command and get output
    result = git.run_git_command(["git", "status", "--porcelain"], str(repo_dir))
    # Empty repo should have empty status
    assert isinstance(result, str)


def test_run_git_command_debug_log_redacts_credentials(
    tmp_path: Path, mock_subprocess_run: Mock, caplog: pytest.LogCaptureFixture
) -> None:
    """Embedded URL credentials never reach the debug log; -v output is
    routinely pasted into public issues. subprocess is mocked so no real
    git ever sees the URL (the path is not creatable on Windows)."""
    mock_subprocess_run.return_value = Mock(returncode=0, stdout=b"", stderr=b"")
    with caplog.at_level(logging.DEBUG, logger="esphome.git"):
        git.run_git_command(
            ["git", "clone", "https://user:hunter2@github.com/test/repo"],
            cwd=tmp_path,
        )
    assert "hunter2" not in caplog.text
    assert "://***@github.com/test/repo" in caplog.text


def test_run_git_command_with_git_dir_isolation(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """Test that git_dir parameter properly isolates git operations."""
    repo_dir = tmp_path / "test_repo"
    repo_dir.mkdir()
    git_dir = repo_dir / ".git"
    git_dir.mkdir()

    # Configure mock to return success
    mock_subprocess_run.return_value = Mock(
        returncode=0,
        stdout=b"test output",
        stderr=b"",
    )

    # Ambient repo-scoping vars simulate a git hook invoking ESPHome; an
    # ambient GIT_INDEX_FILE surviving into a git_dir invocation fails
    # silently (git operates on the caller's index and exits 0).
    with patch.dict(
        os.environ,
        {"GIT_INDEX_FILE": "/caller/index", "GIT_OBJECT_DIRECTORY": "/caller/objects"},
    ):
        result = git.run_git_command(
            ["git", "rev-parse", "HEAD"],
            git_dir=repo_dir,
        )

    # Verify subprocess.run was called
    assert mock_subprocess_run.called
    call_args = mock_subprocess_run.call_args

    # Verify environment was set
    env = call_args[1]["env"]
    assert "GIT_DIR" in env
    assert "GIT_WORK_TREE" in env
    assert env["GIT_DIR"] == str(repo_dir / ".git")
    assert env["GIT_WORK_TREE"] == str(repo_dir)
    # The ambient scoping vars must be stripped, not passed through.
    assert "GIT_INDEX_FILE" not in env
    assert "GIT_OBJECT_DIRECTORY" not in env

    assert result == "test output"


def test_run_git_command_raises_git_not_installed_error(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """Test that FileNotFoundError is converted to GitNotInstalledError."""
    from esphome.git import GitNotInstalledError

    repo_dir = tmp_path / "test_repo"

    # Configure mock to raise FileNotFoundError
    mock_subprocess_run.side_effect = FileNotFoundError("git not found")

    with pytest.raises(GitNotInstalledError, match="git is not installed"):
        git.run_git_command(["git", "status"], git_dir=repo_dir)


def test_run_git_command_raises_git_command_error_on_failure(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """Test that failed git commands raise GitCommandError."""
    repo_dir = tmp_path / "test_repo"

    # Configure mock to return non-zero exit code
    mock_subprocess_run.return_value = Mock(
        returncode=1,
        stdout=b"",
        stderr=b"fatal: not a git repository",
    )

    with pytest.raises(GitCommandError, match="not a git repository"):
        git.run_git_command(["git", "status"], git_dir=repo_dir)


def test_run_git_command_strips_fatal_prefix(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """Test that 'fatal: ' prefix is stripped from error messages."""
    repo_dir = tmp_path / "test_repo"

    # Configure mock to return error with "fatal: " prefix
    mock_subprocess_run.return_value = Mock(
        returncode=128,
        stdout=b"",
        stderr=b"fatal: repository not found\n",
    )

    with pytest.raises(GitCommandError) as exc_info:
        git.run_git_command(["git", "clone", "invalid-url"], git_dir=repo_dir)

    # Error message should NOT include "fatal: " prefix
    assert "fatal:" not in str(exc_info.value)
    assert "repository not found" in str(exc_info.value)


def _git_failure(stderr: bytes, returncode: int = 128) -> Mock:
    """Build a failed subprocess.run result with the given stderr."""
    return Mock(returncode=returncode, stdout=b"", stderr=stderr)


_GIT_OK = Mock(returncode=0, stdout=b"ok", stderr=b"")


def test_run_git_command_network_retries_transient_then_succeeds(
    mock_subprocess_run: Mock,
) -> None:
    """A transient network failure is retried and the retry's result returned."""
    mock_subprocess_run.side_effect = [
        _git_failure(
            b"fatal: unable to access 'https://github.com/test/repo/': "
            b"Could not resolve host: github.com\n"
        ),
        _GIT_OK,
    ]

    with patch("esphome.git.time.sleep") as mock_sleep:
        result = git.run_git_command(
            ["git", "clone", "--depth=1", "--", "https://github.com/test/repo", "x"],
            network=True,
        )

    assert result == "ok"
    assert mock_subprocess_run.call_count == 2
    mock_sleep.assert_called_once_with(2)


def test_run_git_command_network_gives_up_after_max_attempts(
    mock_subprocess_run: Mock,
) -> None:
    """A persistent transient-looking failure raises after the final attempt."""
    mock_subprocess_run.side_effect = lambda *args, **kwargs: _git_failure(
        b"fatal: unable to access 'https://github.com/test/repo/': "
        b"server certificate verification failed. CAfile: none CRLfile: none\n"
    )

    with (
        patch("esphome.git.time.sleep") as mock_sleep,
        pytest.raises(GitCommandError, match="certificate verification failed"),
    ):
        git.run_git_command(["git", "fetch", "--", "origin"], network=True)

    assert mock_subprocess_run.call_count == 3
    assert [c.args[0] for c in mock_sleep.call_args_list] == [2, 4]


@pytest.mark.parametrize(
    ("stderr", "transient"),
    [
        # Transient: DNS, TLS, dropped connections, server-side errors
        ("unable to access 'https://x/': The requested URL returned error: 502", True),
        ("unable to access 'https://x/': Could not resolve host: github.com", True),
        ("unable to access 'https://x/': Failed to connect: Timed out", True),
        ("unable to access 'https://x/': Recv failure: Connection reset", True),
        ("unable to access 'https://x/': Connection refused", True),
        ("fatal: early EOF\nfatal: fetch-pack: invalid index-pack output", True),
        (
            (
                "error: RPC failed; HTTP 500 curl 22 The requested URL returned "
                "error: 500\nfatal: expected flush after ref listing"
            ),
            True,
        ),
        (
            (
                "unable to access 'https://x/': server certificate verification "
                "failed. CAfile: none CRLfile: none"
            ),
            True,
        ),
        (
            (
                "error: RPC failed; curl 56 GnuTLS recv error (-110)\n"
                "fatal: the remote end hung up unexpectedly"
            ),
            True,
        ),
        (
            (
                "fetch-pack: unexpected disconnect while reading sideband packet\n"
                "fatal: early EOF"
            ),
            True,
        ),
        # 429 rate limiting is the one retryable 4xx, in both curl forms
        ("unable to access 'https://x/': The requested URL returned error: 429", True),
        ("error: RPC failed; HTTP 429 curl 22\nfatal: expected flush", True),
        (
            (
                "unable to access 'https://x/': OpenSSL SSL_read: error:0A000126:"
                "SSL routines::unexpected eof while reading, errno 0"
            ),
            True,
        ),
        # Permanent: missing repo, auth, bad ref, other 4xx
        ("fatal: repository 'https://github.com/test/repo/' not found", False),
        (
            (
                "fatal: could not read Username for 'https://github.com': "
                "terminal prompts disabled"
            ),
            False,
        ),
        ("fatal: couldn't find remote ref refs/heads/nope", False),
        (
            (
                "unable to access 'https://github.com/org/private.git/': "
                "The requested URL returned error: 403"
            ),
            False,
        ),
        ("fatal: Authentication failed for 'https://github.com/test/repo/'", False),
        # Smart-HTTP (HTTP/2) 4xx form has no "returned error:" text and
        # mixes in transient-looking wording; still permanent
        (
            (
                "error: RPC failed; HTTP 403 curl 92 HTTP/2 stream 5 was not "
                "closed cleanly: CANCEL (err 8)\nfatal: expected flush after "
                "ref listing"
            ),
            False,
        ),
        (
            (
                "error: RPC failed; HTTP 404 curl 22\n"
                "fatal: the remote end hung up unexpectedly"
            ),
            False,
        ),
        (
            (
                "fatal: unable to access 'https://x/': gnutls_handshake() "
                "failed: The TLS connection was non-properly terminated."
            ),
            True,
        ),
        # Transient-looking tokens in the URL must not classify as transient
        ("fatal: repository 'https://github.com/x/esp32_ssl_reader/' not found", False),
        ("fatal: repository 'https://gitlab.com/gnutls/gnutls.git/' not found", False),
        ("", False),
    ],
)
def test_is_transient_git_error(stderr: str, transient: bool) -> None:
    """Real-world stderr outputs classify correctly as transient or permanent."""
    assert git._is_transient_git_error(stderr) is transient


def test_run_git_command_network_no_retry_on_permanent_error(
    mock_subprocess_run: Mock,
) -> None:
    """Permanent failures (missing repo, auth, bad ref) fail on the first try."""
    mock_subprocess_run.return_value = _git_failure(
        b"fatal: repository 'https://github.com/test/repo/' not found\n"
    )

    with (
        patch("esphome.git.time.sleep") as mock_sleep,
        pytest.raises(GitCommandError),
    ):
        git.run_git_command(["git", "fetch", "--", "origin"], network=True)

    assert mock_subprocess_run.call_count == 1
    mock_sleep.assert_not_called()


def test_run_git_command_network_no_retry_when_git_missing(
    mock_subprocess_run: Mock,
) -> None:
    """A missing git binary is not transient and must not be retried."""
    from esphome.git import GitNotInstalledError

    mock_subprocess_run.side_effect = FileNotFoundError("git not found")

    with (
        patch("esphome.git.time.sleep") as mock_sleep,
        pytest.raises(GitNotInstalledError),
    ):
        git.run_git_command(["git", "fetch", "--", "origin"], network=True)

    assert mock_subprocess_run.call_count == 1
    mock_sleep.assert_not_called()


def test_run_git_command_no_retry_by_default(mock_subprocess_run: Mock) -> None:
    """Without network=True even a transient-looking failure is not retried."""
    mock_subprocess_run.return_value = _git_failure(
        b"fatal: unable to access 'https://github.com/test/repo/': "
        b"Could not resolve host: github.com\n"
    )

    with (
        patch("esphome.git.time.sleep") as mock_sleep,
        pytest.raises(GitCommandError),
    ):
        git.run_git_command(["git", "status"])

    assert mock_subprocess_run.call_count == 1
    mock_sleep.assert_not_called()


def test_run_git_command_network_retry_matches_full_stderr_not_last_line(
    mock_subprocess_run: Mock,
) -> None:
    """The transient marker often sits above the final fatal line; the retry
    decision must look at the full stderr, not just the extracted message."""
    mock_subprocess_run.side_effect = [
        _git_failure(
            b"error: RPC failed; curl 56 GnuTLS recv error (-54)\n"
            b"fatal: fetch-pack: invalid index-pack output\n"
        ),
        _GIT_OK,
    ]

    with patch("esphome.git.time.sleep"):
        result = git.run_git_command(["git", "fetch", "--", "origin"], network=True)

    assert result == "ok"
    assert mock_subprocess_run.call_count == 2


def test_run_git_command_retry_warning_redacts_credentials(
    mock_subprocess_run: Mock, caplog: pytest.LogCaptureFixture
) -> None:
    """The retry warning embeds the git error, which embeds the URL; embedded
    credentials must be redacted since warnings end up in pasted logs."""
    mock_subprocess_run.side_effect = [
        _git_failure(
            b"fatal: unable to access 'https://user:hunter2@github.com/test/repo/': "
            b"Could not resolve host: github.com\n"
        ),
        _GIT_OK,
    ]

    with (
        patch("esphome.git.time.sleep"),
        caplog.at_level(logging.WARNING, logger="esphome.git"),
    ):
        git.run_git_command(["git", "fetch", "--", "origin"], network=True)

    assert "hunter2" not in caplog.text
    assert "://***@github.com/test/repo" in caplog.text


def test_run_git_command_clone_retry_removes_leftover_destination(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """A partial clone destination left by a failed attempt is removed before
    the retry, so the retry cannot fail on 'destination path already exists'."""
    dest = tmp_path / "leftover_clone"
    dest.mkdir()
    (dest / "partial").write_text("x")

    mock_subprocess_run.side_effect = [
        _git_failure(
            b"fatal: unable to access 'https://github.com/test/repo/': "
            b"Could not resolve host: github.com\n"
        ),
        _GIT_OK,
    ]

    with patch("esphome.git.time.sleep"):
        result = git.run_git_command(
            [
                "git",
                "clone",
                "--depth=1",
                "--",
                "https://github.com/test/repo",
                str(dest),
            ],
            network=True,
            retry_cleanup=dest,
        )

    assert result == "ok"
    assert mock_subprocess_run.call_count == 2
    assert not dest.exists()


def test_run_git_command_cleanup_failure_reraises_original_error(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """When the pre-retry cleanup fails, the git error stays the reported
    cause instead of being replaced by the cleanup OSError."""
    dest = tmp_path / "leftover_clone"
    dest.mkdir()

    mock_subprocess_run.return_value = _git_failure(
        b"fatal: unable to access 'https://github.com/test/repo/': "
        b"Could not resolve host: github.com\n"
    )

    with (
        patch("esphome.git.rmtree", side_effect=OSError("locked")),
        patch("esphome.git.time.sleep") as mock_sleep,
        pytest.raises(GitCommandError, match="Could not resolve host"),
    ):
        git.run_git_command(
            ["git", "clone", "--depth=1", "--", "https://github.com/test/repo", "x"],
            network=True,
            retry_cleanup=dest,
        )

    assert mock_subprocess_run.call_count == 1
    mock_sleep.assert_not_called()


def test_run_git_command_no_retry_on_empty_stderr_failure(
    mock_subprocess_run: Mock,
) -> None:
    """A failure with no stderr (e.g. git killed by a signal) is not retried."""
    mock_subprocess_run.return_value = _git_failure(b"", returncode=1)

    with (
        patch("esphome.git.time.sleep") as mock_sleep,
        pytest.raises(GitCommandError, match="git exited with code 1"),
    ):
        git.run_git_command(["git", "fetch", "--", "origin"], network=True)

    assert mock_subprocess_run.call_count == 1
    mock_sleep.assert_not_called()


def test_run_git_command_non_utf8_stderr_does_not_crash(
    mock_subprocess_run: Mock,
) -> None:
    """Locale-encoded (non-UTF-8) stderr must not raise UnicodeDecodeError."""
    mock_subprocess_run.return_value = _git_failure(
        b"fatal: repositorio no encontrado \xe9\xff\n"
    )

    with pytest.raises(GitCommandError, match="repositorio no encontrado"):
        git.run_git_command(["git", "fetch", "--", "origin"], network=True)

    assert mock_subprocess_run.call_count == 1


def test_run_git_command_without_git_dir(mock_subprocess_run: Mock) -> None:
    """Test that run_git_command works without git_dir (clone case)."""
    # Configure mock to return success
    mock_subprocess_run.return_value = Mock(
        returncode=0,
        stdout=b"Cloning into 'test_repo'...",
        stderr=b"",
    )

    result = git.run_git_command(["git", "clone", "https://github.com/test/repo"])

    # Verify subprocess.run was called
    assert mock_subprocess_run.called
    call_args = mock_subprocess_run.call_args

    # Verify environment does NOT have GIT_DIR or GIT_WORK_TREE set
    # (it should use the default environment or None)
    env = call_args[1].get("env")
    if env is not None:
        assert "GIT_DIR" not in env
        assert "GIT_WORK_TREE" not in env

    # Verify cwd is None (default)
    assert call_args[1].get("cwd") is None

    assert result == "Cloning into 'test_repo'..."


@pytest.mark.parametrize("relative", [False, True], ids=["absolute", "relative"])
def test_run_git_command_with_cwd_runs_in_dir_without_isolation(
    tmp_path: Path,
    mock_subprocess_run: Mock,
    monkeypatch: pytest.MonkeyPatch,
    relative: bool,
) -> None:
    """The cwd parameter sets the working directory without GIT_DIR/GIT_WORK_TREE.

    Ambient GIT_DIR/GIT_WORK_TREE (e.g. from a git hook or CI wrapper) must be
    stripped too, and GIT_CEILING_DIRECTORIES must stop git from walking up to
    an enclosing repository if the target repo's .git is missing or corrupt.
    Git silently ignores a relative ceiling entry, so the variable must come
    out absolute even when the given cwd is relative.
    """
    repo_dir = tmp_path / "test_repo"
    repo_dir.mkdir()
    if relative:
        monkeypatch.chdir(tmp_path)
        cwd_arg = Path("test_repo")
    else:
        cwd_arg = repo_dir

    mock_subprocess_run.return_value = Mock(
        returncode=0,
        stdout=b"test output",
        stderr=b"",
    )

    with patch.dict(
        os.environ,
        {
            "GIT_DIR": "/ambient/.git",
            "GIT_WORK_TREE": "/ambient",
            "GIT_INDEX_FILE": "/ambient/.git/index",
        },
    ):
        result = git.run_git_command(["git", "submodule", "update"], cwd=cwd_arg)

    call_args = mock_subprocess_run.call_args
    env = call_args[1]["env"]
    assert "GIT_DIR" not in env
    assert "GIT_WORK_TREE" not in env
    assert "GIT_INDEX_FILE" not in env
    ceiling = Path(env["GIT_CEILING_DIRECTORIES"])
    assert ceiling.is_absolute()
    assert ceiling.samefile(tmp_path)
    assert call_args[1]["cwd"] == cwd_arg
    assert result == "test output"


def test_run_git_command_raises_on_nonfatal_stderr(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """Nonzero exit with stderr lacking a fatal: prefix raises with full stderr."""
    mock_subprocess_run.return_value = Mock(
        returncode=1,
        stdout=b"",
        stderr=b"error: pathspec 'nope' did not match any file(s)\n",
    )

    with pytest.raises(GitCommandError, match="did not match"):
        git.run_git_command(["git", "checkout", "nope"], git_dir=tmp_path)


def test_run_git_command_raises_on_nonzero_exit_without_stderr(
    tmp_path: Path, mock_subprocess_run: Mock
) -> None:
    """A nonzero exit must raise even when git printed nothing to stderr.

    Silent nonzero exits were previously treated as success, which is how
    broken checkouts could be cached as complete.
    """
    mock_subprocess_run.return_value = Mock(
        returncode=1,
        stdout=b"",
        stderr=b"",
    )

    with pytest.raises(GitCommandError, match="exited with code 1"):
        git.run_git_command(["git", "submodule", "update"], cwd=tmp_path)


def test_run_git_command_without_git_dir_raises_error(
    mock_subprocess_run: Mock,
) -> None:
    """Test that run_git_command without git_dir can still raise errors."""
    # Configure mock to return error
    mock_subprocess_run.return_value = Mock(
        returncode=128,
        stdout=b"",
        stderr=b"fatal: repository not found\n",
    )

    with pytest.raises(GitCommandError, match="repository not found"):
        git.run_git_command(["git", "clone", "https://invalid.url/repo.git"])


def test_clone_or_update_with_never_refresh(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that NEVER_REFRESH skips updates for existing repos."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create the git repo directory structure
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with current timestamp
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")

    # Call with NEVER_REFRESH
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=git.NEVER_REFRESH,
        domain=domain,
    )

    # Should NOT call git commands since NEVER_REFRESH and repo exists
    mock_run_git_command.assert_not_called()
    assert result_dir == repo_dir
    assert revert is None


def test_clone_or_update_skips_when_core_skip_external_update(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """CORE.skip_external_update short-circuits the refresh for existing repos."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)
    (git_dir / "FETCH_HEAD").write_text("test")

    CORE.skip_external_update = True
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=TimePeriodSeconds(days=1),
        domain=domain,
    )

    mock_run_git_command.assert_not_called()
    assert result_dir == repo_dir
    assert revert is None


def test_clone_or_update_with_refresh_updates_old_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that refresh triggers update for old repos."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create the git repo directory structure
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with old timestamp (2 days ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    old_time = time.time() - 2 * 86400
    fetch_head.touch()  # Create the file
    # Set modification time to 2 days ago
    os.utime(fetch_head, (old_time, old_time))

    # Mock git command responses
    mock_run_git_command.return_value = "abc123"  # SHA for rev-parse

    # Call with refresh=1d (1 day)
    refresh = TimePeriodSeconds(days=1)
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=refresh,
        domain=domain,
    )

    # Should call git fetch and update commands since repo is older than refresh
    assert mock_run_git_command.called
    # Check for fetch command
    fetch_calls = [
        call
        for call in mock_run_git_command.call_args_list
        if len(call[0]) > 0 and "fetch" in call[0][0]
    ]
    assert len(fetch_calls) > 0


def test_clone_or_update_with_refresh_skips_fresh_repo(
    tmp_path: Path, mock_run_git_command: Mock, caplog: pytest.LogCaptureFixture
) -> None:
    """Test that refresh doesn't update fresh repos."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create the git repo directory structure
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with recent timestamp (1 hour ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    recent_time = time.time() - 3600
    fetch_head.touch()  # Create the file
    # Set modification time to 1 hour ago
    os.utime(fetch_head, (recent_time, recent_time))

    # Freeze the clock at 1 hour (plus a margin larger than any filesystem
    # mtime rounding) after the mtime so the logged countdown is deterministic
    frozen_now = fetch_head.stat().st_mtime + 3600.5

    # Call with refresh=1d (1 day)
    refresh = TimePeriodSeconds(days=1)
    with (
        patch("esphome.git.time.time", return_value=frozen_now),
        caplog.at_level(logging.INFO, logger="esphome.git"),
    ):
        result_dir, revert = git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    # Should NOT call git fetch since repo is fresh
    mock_run_git_command.assert_not_called()
    assert result_dir == repo_dir
    assert revert is None

    # Should tell the user the update was skipped and when the next refresh is
    assert f"Skipping update for {url}@{ref}" in caplog.text
    assert "will refresh on the next run after 22h 59min" in caplog.text
    assert "(refresh: 1d)" in caplog.text


def test_clone_or_update_with_refresh_never_logs_refresh_disabled(
    tmp_path: Path, mock_run_git_command: Mock, caplog: pytest.LogCaptureFixture
) -> None:
    """Test that a config-level refresh: never skips without a countdown log."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create the git repo directory structure
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with current timestamp
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")

    # refresh: never validates to 365250 days, not the NEVER_REFRESH sentinel
    refresh = cv.source_refresh("never")
    with caplog.at_level(logging.DEBUG, logger="esphome.git"):
        result_dir, revert = git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    mock_run_git_command.assert_not_called()
    assert result_dir == repo_dir
    assert revert is None

    # Should log refresh disabled at debug level, not a countdown
    assert f"Skipping update for {url}@{ref} (refresh disabled)" in caplog.text
    assert "will refresh on the next run" not in caplog.text


def test_clone_or_update_clones_missing_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that missing repos are cloned regardless of refresh setting."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create base directory but NOT the repo itself
    base_dir = tmp_path / ".esphome" / domain
    base_dir.mkdir(parents=True)
    # repo_dir should NOT exist
    assert not repo_dir.exists()

    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    # Test with NEVER_REFRESH - should still clone since repo doesn't exist
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=git.NEVER_REFRESH,
        domain=domain,
    )

    # Should call git clone
    assert mock_run_git_command.called
    clone_calls = [
        call
        for call in mock_run_git_command.call_args_list
        if len(call[0]) > 0 and "clone" in call[0][0]
    ]
    assert len(clone_calls) > 0


def test_clone_or_update_with_none_refresh_always_updates(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that refresh=None always updates existing repos."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = None
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Create the git repo directory structure
    repo_dir.mkdir(parents=True)
    git_dir = repo_dir / ".git"
    git_dir.mkdir()
    _mark_clone_complete(repo_dir)

    # Create FETCH_HEAD file with very recent timestamp (1 second ago)
    fetch_head = git_dir / "FETCH_HEAD"
    fetch_head.write_text("test")
    recent_time = time.time() - 1
    fetch_head.touch()  # Create the file
    # Set modification time to 1 second ago
    os.utime(fetch_head, (recent_time, recent_time))

    # Mock git command responses
    mock_run_git_command.return_value = "abc123"  # SHA for rev-parse

    # Call with refresh=None (default behavior)
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=None,
        domain=domain,
    )

    # Should call git fetch and update commands since refresh=None means always update
    assert mock_run_git_command.called
    # Check for fetch command
    fetch_calls = [
        call
        for call in mock_run_git_command.call_args_list
        if len(call[0]) > 0 and "fetch" in call[0][0]
    ]
    assert len(fetch_calls) > 0


@pytest.mark.parametrize(
    ("fail_command", "error_message"),
    [
        (
            "rev-parse",
            "ambiguous argument 'HEAD': unknown revision or path not in the working tree.",
        ),
        ("stash", "fatal: unable to write new index file"),
        # The fetch failure must be non-transient: a transient one (e.g.
        # "Could not resolve host") now keeps the existing clone instead of
        # triggering recovery.
        ("fetch", "fatal: couldn't find remote ref main"),
        ("reset", "fatal: Could not reset index file to revision 'FETCH_HEAD'"),
    ],
)
def test_clone_or_update_recovers_from_git_failures(
    tmp_path: Path, mock_run_git_command: Mock, fail_command: str, error_message: str
) -> None:
    """Test that repos are re-cloned when various git commands fail."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Use helper to set up old repo
    _setup_old_repo(repo_dir)

    # Track command call counts to make first call fail, subsequent calls succeed
    call_counts: dict[str, int] = {}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        # Determine which command this is
        cmd_type = _get_git_command_type(cmd)

        # Track call count for this command type
        if cmd_type:
            call_counts[cmd_type] = call_counts.get(cmd_type, 0) + 1

        # Fail on first call to the specified command, succeed on subsequent calls
        if cmd_type == fail_command and call_counts[cmd_type] == 1:
            raise GitCommandError(error_message)

        # Default successful responses
        if cmd_type == "rev-parse":
            return "abc123"
        if cmd_type == "clone":
            # Simulate the recovery re-clone creating the repo directory
            _simulate_cloned_repo(repo_dir)
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)
    result_dir, revert = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=refresh,
        domain=domain,
    )

    # Verify recovery happened
    call_list = mock_run_git_command.call_args_list

    # Should have attempted the failing command
    assert any(fail_command in str(c) for c in call_list)

    # Should have called clone for recovery
    assert any("clone" in str(c) for c in call_list)

    # Verify the repo directory path is returned
    assert result_dir == repo_dir


@pytest.mark.parametrize("fetch_head_preexists", [True, False])
def test_clone_or_update_transient_fetch_keeps_existing_clone(
    tmp_path: Path,
    mock_run_git_command: Mock,
    caplog: pytest.LogCaptureFixture,
    fetch_head_preexists: bool,
) -> None:
    """A transient network failure while refreshing a verified clone falls back
    to the existing clone instead of destroying it with a recovery re-clone."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)
    _setup_old_repo(repo_dir)
    if not fetch_head_preexists:
        # First-ever refresh: age comes from HEAD, FETCH_HEAD absent
        (repo_dir / ".git" / "FETCH_HEAD").unlink()
        head = repo_dir / ".git" / "HEAD"
        head.write_text("test")
        old_time = time.time() - 2 * 86400
        os.utime(head, (old_time, old_time))

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            return "abc123"
        if cmd_type == "fetch":
            # A failed fetch still freshens FETCH_HEAD, like real git
            (repo_dir / ".git" / "FETCH_HEAD").touch()
            stderr = (
                "fatal: unable to access "
                "'https://user:hunter2@github.com/test/repo/': "
                "Could not resolve host: github.com"
            )
            raise GitCommandError(stderr, stderr=stderr)
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)
    with caplog.at_level(logging.WARNING, logger="esphome.git"):
        result_dir, revert = git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    # The existing clone is returned, not removed or re-cloned
    assert result_dir == repo_dir
    assert repo_dir.is_dir()
    assert revert is None
    assert not any(
        _get_git_command_type(c[0][0]) == "clone"
        for c in mock_run_git_command.call_args_list
    )
    # The completion marker must be restored, or the next run treats the
    # entry as an incomplete clone and removes it
    assert _marker_path(repo_dir).is_file()
    # The warning must say what the build will actually use and how stale it is
    assert "using the existing clone at abc123" in caplog.text
    assert "ago" in caplog.text
    # Credentials embedded in the URL must not reach the warning log
    assert "hunter2" not in caplog.text
    assert "://***@github.com/test/repo" in caplog.text
    # The FETCH_HEAD the failed fetch freshened must not survive, or the
    # refresh window would suppress retrying the update on subsequent runs
    fetch_head = repo_dir / ".git" / "FETCH_HEAD"
    if fetch_head_preexists:
        assert time.time() - fetch_head.stat().st_mtime > refresh.total_seconds
    else:
        assert not fetch_head.exists()


def test_clone_or_update_timestamp_restore_failure_routes_to_recovery(
    tmp_path: Path, mock_run_git_command: Mock, caplog: pytest.LogCaptureFixture
) -> None:
    """If the FETCH_HEAD restore fails, the fallback cannot stay honest, so
    the git error must route through recovery instead of a raw OSError."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)
    _setup_old_repo(repo_dir)

    call_counts: dict[str, int] = {}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type:
            call_counts[cmd_type] = call_counts.get(cmd_type, 0) + 1
        if cmd_type == "rev-parse":
            return "abc123"
        if cmd_type == "fetch" and call_counts[cmd_type] == 1:
            stderr = (
                "fatal: unable to access 'https://github.com/test/repo/': "
                "Could not resolve host: github.com"
            )
            raise GitCommandError(stderr, stderr=stderr)
        if cmd_type == "clone":
            _simulate_cloned_repo(repo_dir)
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)
    with (
        patch("esphome.git.os.utime", side_effect=OSError("read-only")),
        caplog.at_level(logging.WARNING, logger="esphome.git"),
    ):
        result_dir, _ = git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    assert result_dir == repo_dir
    assert "Could not restore the refresh timestamp" in caplog.text
    # Recovery re-cloned rather than surfacing the OSError
    assert call_counts.get("clone", 0) == 1


@pytest.mark.parametrize(
    "refresh", [None, TimePeriodSeconds(days=1)], ids=["clone", "refresh"]
)
def test_clone_or_update_network_commands_carry_retry_flag(
    tmp_path: Path, mock_run_git_command: Mock, refresh: TimePeriodSeconds | None
) -> None:
    """clone/fetch/submodule opt into transient-failure retry; local commands
    (rev-parse, stash, reset) must not, so a refactor cannot silently drop or
    widen the retry wiring."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    if refresh is None:
        mock_run_git_command.side_effect = _make_clone_side_effect(
            repo_dir, gitmodules=True
        )
    else:
        _setup_old_repo(repo_dir)
        (repo_dir / ".gitmodules").write_text("test")
        mock_run_git_command.return_value = "abc123"

    git.clone_or_update(
        url=url,
        ref=ref,
        refresh=refresh,
        domain=domain,
        init_submodules=True,
    )

    seen: set[str] = set()
    for call in mock_run_git_command.call_args_list:
        cmd_type = _get_git_command_type(call.args[0])
        seen.add(cmd_type)
        if cmd_type in ("clone", "fetch", "submodule"):
            assert call.kwargs.get("network") is True, cmd_type
        else:
            assert "network" not in call.kwargs, cmd_type
        if cmd_type == "clone":
            assert call.kwargs.get("retry_cleanup") == repo_dir

    expected = {"fetch", "reset", "submodule"}
    expected |= {"clone"} if refresh is None else {"rev-parse", "stash"}
    assert expected <= seen


def test_clone_or_update_transient_submodule_failure_still_recovers(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A transient failure after the reset (submodules) leaves a half-updated
    tree, so it must route through recovery instead of keeping the clone."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    _setup_old_repo(repo_dir)
    (repo_dir / ".gitmodules").write_text("test")

    call_counts: dict[str, int] = {}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type:
            call_counts[cmd_type] = call_counts.get(cmd_type, 0) + 1
        if cmd_type == "rev-parse":
            return "abc123"
        if cmd_type == "submodule" and call_counts[cmd_type] == 1:
            stderr = (
                "fatal: unable to access 'https://github.com/test/sub/': "
                "Could not resolve host: github.com"
            )
            raise GitCommandError(stderr, stderr=stderr)
        if cmd_type == "clone":
            _simulate_cloned_repo(repo_dir)
            (repo_dir / ".gitmodules").write_text("test")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)
    result_dir, _ = git.clone_or_update(
        url=url,
        ref=None,
        refresh=refresh,
        domain=domain,
        init_submodules=True,
    )

    assert result_dir == repo_dir
    # The half-updated tree must be recovered via re-clone, not kept
    assert call_counts.get("clone", 0) == 1


def test_clone_or_update_fails_when_recovery_also_fails(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that we don't infinitely recurse when recovery also fails."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Use helper to set up old repo
    _setup_old_repo(repo_dir)

    # Mock git command to fail on clone (simulating network failure during recovery)
    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            # First time fails (broken repo)
            raise GitCommandError(
                "ambiguous argument 'HEAD': unknown revision or path not in the working tree."
            )
        if cmd_type == "clone":
            # Clone also fails (recovery fails)
            raise GitCommandError("fatal: unable to access repository")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # Should raise after one recovery attempt fails
    with pytest.raises(GitCommandError, match="fatal: unable to access repository"):
        git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    # Verify we only tried to clone once (no infinite recursion)
    call_list = mock_run_git_command.call_args_list
    clone_calls = [c for c in call_list if "clone" in c[0][0]]
    # Should have exactly one clone call (the recovery attempt that failed)
    assert len(clone_calls) == 1
    # Should have tried rev-parse once (which failed and triggered recovery)
    rev_parse_calls = [c for c in call_list if "rev-parse" in c[0][0]]
    assert len(rev_parse_calls) == 1


def test_clone_or_update_recover_broken_flag_prevents_second_recovery(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that _recover_broken=False prevents a second recovery attempt (tests the raise path)."""
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Use helper to set up old repo
    _setup_old_repo(repo_dir)

    # Track fetch calls to differentiate between first (in clone) and second (in recovery update)
    call_counts: dict[str, int] = {}

    # Mock git command to fail on fetch during recovery's ref checkout
    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)

        if cmd_type:
            call_counts[cmd_type] = call_counts.get(cmd_type, 0) + 1

        # First attempt: rev-parse fails (broken repo)
        if cmd_type == "rev-parse" and call_counts[cmd_type] == 1:
            raise GitCommandError(
                "ambiguous argument 'HEAD': unknown revision or path not in the working tree."
            )

        # Recovery: clone succeeds
        if cmd_type == "clone":
            return ""

        # Recovery: fetch for ref checkout fails
        # This happens in the clone path when ref is not None (line 80 in git.py)
        if cmd_type == "fetch" and call_counts[cmd_type] == 1:
            raise GitCommandError("fatal: couldn't find remote ref main")

        # Default success
        return "abc123" if cmd_type == "rev-parse" else ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # Should raise on the fetch during recovery (when _recover_broken=False)
    # This tests the critical "if not _recover_broken: raise" path
    with pytest.raises(GitCommandError, match="fatal: couldn't find remote ref main"):
        git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    # Verify the sequence of events
    call_list = mock_run_git_command.call_args_list

    # Should have: rev-parse (fail, triggers recovery), clone (success),
    # fetch (fail during ref checkout, raises because _recover_broken=False)
    rev_parse_calls = [c for c in call_list if "rev-parse" in c[0][0]]
    # Should have exactly one rev-parse call that failed
    assert len(rev_parse_calls) == 1

    clone_calls = [c for c in call_list if "clone" in c[0][0]]
    # Should have exactly one clone call (the recovery attempt)
    assert len(clone_calls) == 1

    fetch_calls = [c for c in call_list if "fetch" in c[0][0]]
    # Should have exactly one fetch call that failed (during ref checkout in recovery)
    assert len(fetch_calls) == 1


def test_clone_or_update_recover_broken_flag_prevents_infinite_loop(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that _recover_broken=False prevents infinite recursion when repo persists."""
    # This tests the critical "if not _recover_broken: raise" path at line 124-125
    # Set up CORE.config_path so data_dir uses tmp_path
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Use helper to set up old repo
    _setup_old_repo(repo_dir)

    # Mock shutil.rmtree to NOT actually delete the directory
    # This simulates a scenario where deletion fails (permissions, etc.)
    import unittest.mock

    def mock_rmtree(path, *args, **kwargs):
        # Don't actually delete - this causes the recursive call to still see the repo
        pass

    # Mock git commands to always fail on stash
    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            return "abc123"
        if cmd_type == "stash":
            # Always fails
            raise GitCommandError("fatal: unable to write new index file")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # Mock shutil.rmtree and test
    # Should raise on the second attempt when _recover_broken=False
    # This hits the "if not _recover_broken: raise" path
    with (
        unittest.mock.patch("esphome.git.rmtree", side_effect=mock_rmtree),
        pytest.raises(GitCommandError, match="fatal: unable to write new index file"),
    ):
        git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    # Verify the sequence: stash fails twice (once triggering recovery, once raising)
    call_list = mock_run_git_command.call_args_list
    stash_calls = [c for c in call_list if "stash" in c[0][0]]
    # Should have exactly two stash calls
    assert len(stash_calls) == 2


def test_clone_or_update_cleans_up_on_failed_ref_fetch(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that a failed ref fetch removes the incomplete clone directory.

    When cloning with a specific ref, if `git clone` succeeds but the
    subsequent `git fetch <ref>` fails, the clone directory should be
    removed so the next attempt starts fresh instead of finding a stale
    clone on the default branch.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "pull/123/head"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "clone":
            # Simulate successful clone by creating the directory
            repo_dir.mkdir(parents=True, exist_ok=True)
            (repo_dir / ".git").mkdir(exist_ok=True)
            return ""
        if cmd_type == "fetch":
            raise GitCommandError("fatal: couldn't find remote ref pull/123/head")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    with pytest.raises(GitCommandError, match="couldn't find remote ref"):
        git.clone_or_update(
            url=url,
            ref=ref,
            refresh=refresh,
            domain=domain,
        )

    # The incomplete clone directory should have been removed
    assert not repo_dir.exists()

    # Verify clone was attempted then fetch failed
    call_list = mock_run_git_command.call_args_list
    clone_calls = [c for c in call_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    fetch_calls = [c for c in call_list if "fetch" in c[0][0]]
    assert len(fetch_calls) == 1


def test_clone_or_update_stale_clone_is_retried_after_cleanup(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Test that after cleanup, a subsequent call does a fresh clone.

    This is the full scenario: first call fails at fetch (directory cleaned up),
    second call sees no directory and clones fresh.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "pull/123/head"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    call_count = {"clone": 0, "fetch": 0}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "clone":
            call_count["clone"] += 1
            repo_dir.mkdir(parents=True, exist_ok=True)
            (repo_dir / ".git").mkdir(exist_ok=True)
            return ""
        if cmd_type == "fetch":
            call_count["fetch"] += 1
            if call_count["fetch"] == 1:
                # First fetch fails
                raise GitCommandError("fatal: couldn't find remote ref pull/123/head")
            # Second fetch succeeds
            return ""
        if cmd_type == "reset":
            return ""
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    refresh = TimePeriodSeconds(days=1)

    # First call: clone succeeds, fetch fails, directory cleaned up
    with pytest.raises(GitCommandError, match="couldn't find remote ref"):
        git.clone_or_update(url=url, ref=ref, refresh=refresh, domain=domain)

    assert not repo_dir.exists()

    # Second call: fresh clone + fetch succeeds
    result_dir, _ = git.clone_or_update(
        url=url, ref=ref, refresh=refresh, domain=domain
    )

    assert result_dir == repo_dir
    assert repo_dir.exists()
    assert call_count["clone"] == 2
    assert call_count["fetch"] == 2


def test_clone_or_update_recloned_when_marker_missing_with_never_refresh(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A repo dir without the completion marker is an interrupted clone.

    It must be removed and re-cloned even with NEVER_REFRESH, which would
    otherwise trust the broken directory forever.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "1.8.4"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    # Simulate an interrupted clone: directory exists, no marker
    repo_dir.mkdir(parents=True)
    (repo_dir / ".git").mkdir()

    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    result_dir, _ = git.clone_or_update(
        url=url, ref=ref, refresh=git.NEVER_REFRESH, domain=domain
    )

    clone_calls = [c for c in mock_run_git_command.call_args_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    assert result_dir == repo_dir
    # The fresh clone completed, so the marker must now be present
    assert _marker_path(repo_dir).is_file()


def test_clone_or_update_recloned_when_marker_missing_with_skip_external_update(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """skip_external_update must not preserve an interrupted clone."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    repo_dir.mkdir(parents=True)
    (repo_dir / ".git").mkdir()

    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    CORE.skip_external_update = True
    result_dir, _ = git.clone_or_update(
        url=url, ref=None, refresh=TimePeriodSeconds(days=1), domain=domain
    )

    clone_calls = [c for c in mock_run_git_command.call_args_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    assert result_dir == repo_dir
    assert _marker_path(repo_dir).is_file()


def test_fresh_clone_writes_completion_marker_with_debug_info(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """The marker is written after a fresh clone and records key and hash dir."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    git.clone_or_update(url=url, ref=ref, refresh=git.NEVER_REFRESH, domain=domain)

    marker = _marker_path(repo_dir)
    assert marker.is_file()
    content = marker.read_text()
    assert f"{url}@{ref}" in content
    assert repo_dir.name in content


def test_marker_is_deleted_before_rmtree(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """The marker must be gone even if rmtree fails partway.

    Simulated by an rmtree that does nothing: the directory survives but the
    marker must already have been deleted, so the next run still re-clones
    instead of trusting a partially deleted worktree.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    _setup_old_repo(repo_dir)
    assert _marker_path(repo_dir).is_file()

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "stash":
            raise GitCommandError("fatal: unable to write new index file")
        return "abc123"

    mock_run_git_command.side_effect = git_command_side_effect

    with (
        patch("esphome.git.rmtree"),
        pytest.raises(GitCommandError),
    ):
        git.clone_or_update(
            url=url, ref=ref, refresh=TimePeriodSeconds(days=1), domain=domain
        )

    # rmtree never deleted anything, yet the marker is gone
    assert repo_dir.is_dir()
    assert not _marker_path(repo_dir).is_file()


def test_failed_marker_write_does_not_fail_the_clone(
    tmp_path: Path,
    mock_run_git_command: Mock,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A marker write failure must not fail an otherwise complete clone.

    The clone is valid; the missing marker only costs a re-clone on the next
    run, so the error is logged as a warning instead of propagating.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    with patch(
        "esphome.git.write_file", side_effect=EsphomeError("Could not write file")
    ):
        result_dir, _ = git.clone_or_update(
            url=url, ref=None, refresh=git.NEVER_REFRESH, domain=domain
        )

    assert result_dir == repo_dir
    assert not _marker_path(repo_dir).is_file()
    assert "Could not write clone completion marker" in caplog.text


def test_corrupt_git_dir_without_head_recovers(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A .git with neither FETCH_HEAD nor HEAD must recover, not crash.

    The age check stats FETCH_HEAD falling back to HEAD; if both are gone
    (partially deleted clone) the stat raised an unhandled FileNotFoundError
    before the broken-repository recovery could run.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    # Marker present but .git gutted: no FETCH_HEAD, no HEAD
    repo_dir.mkdir(parents=True)
    (repo_dir / ".git").mkdir()
    _mark_clone_complete(repo_dir)

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            raise GitCommandError("ambiguous argument 'HEAD': unknown revision")
        if cmd_type == "clone":
            _simulate_cloned_repo(repo_dir)
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    result_dir, _ = git.clone_or_update(
        url=url, ref=None, refresh=TimePeriodSeconds(days=1), domain=domain
    )

    assert result_dir == repo_dir
    clone_calls = [c for c in mock_run_git_command.call_args_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    assert _marker_path(repo_dir).is_file()


def test_remove_repo_dir_tolerates_marker_unlink_failure(tmp_path: Path) -> None:
    """A locked marker file must not abort the directory removal."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    (repo_dir / ".git").mkdir()
    _mark_clone_complete(repo_dir)

    with patch.object(Path, "unlink", side_effect=PermissionError("locked")):
        git._remove_repo_dir(repo_dir)

    # rmtree still removed the directory, marker included
    assert not repo_dir.exists()


def test_clone_or_update_recovery_preserves_subpath(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Recovery must re-clone into the same subpath-ed directory.

    Without passing subpath through, the recursive recovery call would
    recompute the destination without the subpath and clone (and write the
    completion marker) at the wrong location.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    subpath = Path("mylib")
    repo_dir = _compute_repo_dir(url, ref, domain) / subpath

    _setup_old_repo(repo_dir)

    call_counts: dict[str, int] = {}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type:
            call_counts[cmd_type] = call_counts.get(cmd_type, 0) + 1
        # First rev-parse fails (broken repo) to trigger recovery
        if cmd_type == "rev-parse" and call_counts[cmd_type] == 1:
            raise GitCommandError(
                "ambiguous argument 'HEAD': unknown revision or path not in the working tree."
            )
        if cmd_type == "clone":
            # Create whatever directory the clone was asked to target
            target = Path(cmd[-1])
            target.mkdir(parents=True, exist_ok=True)
            (target / ".git").mkdir(exist_ok=True)
        if cmd_type == "rev-parse":
            return "abc123"
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    result_dir, _ = git.clone_or_update(
        url=url,
        ref=ref,
        refresh=TimePeriodSeconds(days=1),
        domain=domain,
        subpath=subpath,
    )

    # The recovery re-clone must target the subpath-ed directory and the
    # completion marker must land there too
    clone_calls = [c for c in mock_run_git_command.call_args_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    assert clone_calls[0][0][0][-1] == str(repo_dir)
    assert result_dir == repo_dir
    assert _marker_path(repo_dir).is_file()


def test_clone_with_ref_uses_shallow_fetch(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Clone with a ref should use --depth=1 on both clone and fetch."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "pull/123/head"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "clone":
            repo_dir.mkdir(parents=True, exist_ok=True)
            (repo_dir / ".git").mkdir(exist_ok=True)
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    git.clone_or_update(url=url, ref=ref, refresh=None, domain=domain)

    call_list = mock_run_git_command.call_args_list

    clone_calls = [c for c in call_list if "clone" in c[0][0]]
    assert len(clone_calls) == 1
    assert "--depth=1" in clone_calls[0][0][0]

    fetch_calls = [c for c in call_list if "fetch" in c[0][0]]
    assert len(fetch_calls) == 1
    assert "--depth=1" in fetch_calls[0][0][0]
    # Ref must still be passed so the requested commit/branch is fetched.
    assert ref in fetch_calls[0][0][0]


def test_refresh_fetch_is_shallow(tmp_path: Path, mock_run_git_command: Mock) -> None:
    """The refresh-path fetch should use --depth=1."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    _setup_old_repo(repo_dir)
    mock_run_git_command.return_value = "abc123"

    git.clone_or_update(
        url=url, ref=ref, refresh=TimePeriodSeconds(days=1), domain=domain
    )

    fetch_calls = [c for c in mock_run_git_command.call_args_list if "fetch" in c[0][0]]
    assert len(fetch_calls) == 1
    cmd = fetch_calls[0][0][0]
    assert "--depth=1" in cmd
    # Ref must still be in the refresh fetch so the right tip is updated.
    assert cmd[-1] == ref


@pytest.mark.parametrize(
    "refresh", [None, TimePeriodSeconds(days=1)], ids=["clone", "refresh"]
)
def test_all_submodules_skipped_without_gitmodules(
    tmp_path: Path, mock_run_git_command: Mock, refresh: TimePeriodSeconds | None
) -> None:
    """init_submodules is a no-op for repos with no .gitmodules.

    This is the esp-idf toolchain library scenario from issue #17860: the
    PlatformIO library converter requests "all submodules" for every git
    library, and most libraries declare none. The git submodule porcelain
    must not run at all in that case — it fails outright on some git
    installations.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    if refresh is None:
        mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)
    else:
        _setup_old_repo(repo_dir)
        mock_run_git_command.return_value = "abc123"

    git.clone_or_update(
        url=url,
        ref=None,
        refresh=refresh,
        domain=domain,
        init_submodules=True,
    )

    assert not _submodule_calls(mock_run_git_command)


@pytest.mark.parametrize(
    "refresh", [None, TimePeriodSeconds(days=1)], ids=["clone", "refresh"]
)
def test_all_submodules_updated_with_gitmodules(
    tmp_path: Path, mock_run_git_command: Mock, refresh: TimePeriodSeconds | None
) -> None:
    """init_submodules initializes all submodules when .gitmodules exists."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    if refresh is None:
        mock_run_git_command.side_effect = _make_clone_side_effect(
            repo_dir, gitmodules=True
        )
    else:
        _setup_old_repo(repo_dir)
        (repo_dir / ".gitmodules").write_text("test")
        mock_run_git_command.return_value = "abc123"

    git.clone_or_update(
        url=url,
        ref=None,
        refresh=refresh,
        domain=domain,
        init_submodules=True,
    )

    submodule_calls = _submodule_calls(mock_run_git_command)
    # Which submodules get populated is git's own policy, so no status
    # verification follows the update.
    assert len(submodule_calls) == 1
    cmd = submodule_calls[0][0][0]
    assert cmd[2] == "update"
    assert "--depth=1" in cmd
    # Recursive, mirroring PlatformIO's recursive library clones.
    assert "--recursive" in cmd
    _assert_submodule_runs_without_isolation(submodule_calls[0], repo_dir)


def test_recovery_reclone_keeps_credentials_and_cache_key(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """The recovery re-clone must not re-apply credentials to the already
    rewritten URL (no doubled userinfo) and must land in the same cache
    directory, or a credentialed private repo re-clones on every run."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    _setup_old_repo(repo_dir)
    (repo_dir / ".gitmodules").write_text("test")

    calls = {"submodule": 0}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "clone":
            _simulate_cloned_repo(repo_dir)
        if _get_git_command_type(cmd) == "submodule":
            calls["submodule"] += 1
            if calls["submodule"] == 1:
                raise git.GitCommandError("git submodule update exited with code 1")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    recovered_dir, _ = git.clone_or_update(
        url=url,
        ref=None,
        refresh=TimePeriodSeconds(days=1),
        domain=domain,
        username="user",
        password="hunter2",
        init_submodules=True,
    )

    assert recovered_dir == repo_dir
    clone_cmds = [
        c[0][0]
        for c in mock_run_git_command.call_args_list
        if _get_git_command_type(c[0][0]) == "clone"
    ]
    assert clone_cmds
    clone_url = clone_cmds[0][-2]
    assert clone_url == "https://user:hunter2@github.com/test/repo"
    assert clone_url.count("@") == 1


def test_refresh_submodule_failure_recovers_then_raises(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A refresh-path submodule failure routes through the recovery re-clone.

    The broken repo is removed and re-cloned; when the submodule update fails
    again on the fresh clone the cache entry is removed and the error
    propagates, instead of leaving behind a repo the refresh window would
    silently accept on the next run.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    _setup_old_repo(repo_dir)
    (repo_dir / ".gitmodules").write_text("test")

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "clone":
            _simulate_cloned_repo(repo_dir)
            (repo_dir / ".gitmodules").write_text("test")
        if _get_git_command_type(cmd) == "submodule":
            raise git.GitCommandError("git submodule update exited with code 1")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    with pytest.raises(git.GitCommandError, match="exited with code 1"):
        git.clone_or_update(
            url=url,
            ref=None,
            refresh=TimePeriodSeconds(days=1),
            domain=domain,
            init_submodules=True,
        )

    assert not repo_dir.is_dir()
    # Recovery removed the repo and re-cloned before failing again.
    assert any(
        _get_git_command_type(c[0][0]) == "clone"
        for c in mock_run_git_command.call_args_list
    )


def _lock_path(url: str, ref: str | None, domain: str) -> Path:
    """The lock file the implementation uses for one cache entry."""
    return git._repo_lock_path(git._cache_key(url, ref), domain)


class _SetEventOnWaitLog(logging.Handler):
    """Set an event when the 'Waiting for another process' record is emitted,
    so tests can react to a caller observably blocking on the lock instead of
    racing a wall-clock timer."""

    def __init__(self, event: threading.Event) -> None:
        super().__init__()
        self._event = event

    def emit(self, record: logging.LogRecord) -> None:
        if "Waiting for another process" in record.getMessage():
            self._event.set()


def test_clone_or_update_serializes_concurrent_clones(
    tmp_path: Path, mock_run_git_command: Mock, caplog: pytest.LogCaptureFixture
) -> None:
    """Two concurrent callers for the same uncached repo must not both clone.

    Without the per-entry lock both callers pass the is_dir() check before
    either clone finishes, so the second one either clones on top of the
    first or reads a half populated worktree (device-builder issue 2425).
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)

    start_together = threading.Barrier(2)
    other_caller_waiting = threading.Event()

    def on_clone() -> None:
        # Hold the lock until the other caller is observably blocked on it,
        # so the interleaving is guaranteed rather than raced on a timer.
        assert other_caller_waiting.wait(timeout=30)

    mock_run_git_command.side_effect = _make_clone_side_effect(
        repo_dir, on_clone=on_clone
    )

    results: list[Path] = []
    errors: list[BaseException] = []

    def call() -> None:
        try:
            start_together.wait()
            result_dir, _ = git.clone_or_update(
                url=url, ref=None, refresh=git.NEVER_REFRESH, domain=domain
            )
            results.append(result_dir)
        except BaseException as err:  # noqa: BLE001 - re-raised via errors below
            errors.append(err)

    handler = _SetEventOnWaitLog(other_caller_waiting)
    git_logger = logging.getLogger("esphome.git")
    git_logger.addHandler(handler)
    try:
        with caplog.at_level(logging.INFO):
            threads = [threading.Thread(target=call) for _ in range(2)]
            for t in threads:
                t.start()
            for t in threads:
                t.join()
    finally:
        git_logger.removeHandler(handler)

    assert not errors
    assert results == [repo_dir, repo_dir]
    clone_calls = [
        c
        for c in mock_run_git_command.call_args_list
        if _get_git_command_type(c[0][0]) == "clone"
    ]
    # The second caller waited for the lock, then saw the completed clone.
    assert len(clone_calls) == 1
    assert _marker_path(repo_dir).is_file()


def test_clone_or_update_creates_lock_file_next_to_hash_dir(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """The lock file lives beside the hash dir, never inside it.

    Checked while the clone runs (the lock is held): filelock's Windows
    backend deletes the lock file on release, so probing after the call
    would only work on Unix.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    lock_path = _lock_path(url, None, domain)
    lock_held_during_clone: list[bool] = []

    mock_run_git_command.side_effect = _make_clone_side_effect(
        repo_dir, on_clone=lambda: lock_held_during_clone.append(lock_path.is_file())
    )

    result_dir, _ = git.clone_or_update(
        url=url, ref=None, refresh=git.NEVER_REFRESH, domain=domain
    )

    assert result_dir == repo_dir
    assert lock_path.parent == repo_dir.parent
    assert lock_held_during_clone == [True]


def test_clone_or_update_subpath_locks_at_hash_dir_level(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """With a subpath the lock still guards the whole hash dir cache entry."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    hash_dir = _compute_repo_dir(url, None, domain)
    repo_dir = hash_dir / "lib"
    lock_path = _lock_path(url, None, domain)
    lock_held_during_clone: list[bool] = []

    mock_run_git_command.side_effect = _make_clone_side_effect(
        repo_dir, on_clone=lambda: lock_held_during_clone.append(lock_path.is_file())
    )

    result_dir, _ = git.clone_or_update(
        url=url,
        ref=None,
        refresh=git.NEVER_REFRESH,
        domain=domain,
        subpath=Path("lib"),
    )

    assert result_dir == repo_dir
    assert lock_path == hash_dir.parent / f"{hash_dir.name}.lock"
    assert lock_held_during_clone == [True]


def test_clone_or_update_recovery_holds_lock_without_deadlock(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Recovery re-clones while holding the lock and must not re-acquire it.

    A naive re-acquisition would deadlock here, since OS file locks taken on
    separate descriptors conflict even within one process. The lock file
    itself must survive the recovery rmtree of the broken repo dir: the
    re-clone runs after the rmtree, so probing the lock file there proves
    it. Probing after the call would only work on Unix, since filelock's
    Windows backend deletes the lock file on release.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    lock_path = _lock_path(url, None, domain)
    lock_held_during_reclone: list[bool] = []

    _setup_old_repo(repo_dir)

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "stash":
            raise git.GitCommandError("broken repository")
        if _get_git_command_type(cmd) == "clone":
            lock_held_during_reclone.append(lock_path.is_file())
            _simulate_cloned_repo(repo_dir)
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    recovered_dir, _ = git.clone_or_update(
        url=url, ref=None, refresh=TimePeriodSeconds(days=1), domain=domain
    )

    assert recovered_dir == repo_dir
    assert lock_held_during_reclone == [True]


def _hold_lock_in_thread(lock_path: Path) -> tuple[threading.Thread, threading.Event]:
    """Hold the lock from another thread; returns the thread and its release event.

    OS file locks taken on separate descriptors conflict even within one
    process, so a second FileLock instance in a thread contends the same
    way another process would.
    """
    held = threading.Event()
    release = threading.Event()

    def hold() -> None:
        with FileLock(str(lock_path)):
            held.set()
            release.wait(timeout=30)

    holder = threading.Thread(target=hold)
    holder.start()
    assert held.wait(timeout=30)
    return holder, release


def test_clone_or_update_logs_wait_on_contended_lock(
    tmp_path: Path, mock_run_git_command: Mock, caplog: pytest.LogCaptureFixture
) -> None:
    """A contended acquire logs a redacted waiting message instead of
    silently blocking."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://user:hunter2@github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    holder, release = _hold_lock_in_thread(_lock_path(url, None, domain))

    # Free the lock only once the waiting message has been emitted, so the
    # release is caused by the thing being asserted instead of racing it.
    handler = _SetEventOnWaitLog(release)
    git_logger = logging.getLogger("esphome.git")
    git_logger.addHandler(handler)
    try:
        with caplog.at_level(logging.INFO):
            result_dir, _ = git.clone_or_update(
                url=url, ref=None, refresh=git.NEVER_REFRESH, domain=domain
            )
    finally:
        git_logger.removeHandler(handler)
        release.set()
    holder.join()

    assert result_dir == repo_dir
    waiting = [
        r.getMessage()
        for r in caplog.records
        if "Waiting for another process" in r.getMessage()
    ]
    assert len(waiting) == 1
    assert "hunter2" not in waiting[0]
    assert "://***@" in waiting[0]


def test_revert_skips_on_contended_lock(
    tmp_path: Path,
    mock_run_git_command: Mock,
    caplog: pytest.LogCaptureFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """revert() only runs on an already-failing path; when it cannot get the
    lock within the bounded timeout it warns and skips instead of hanging."""
    CORE.config_path = tmp_path / "test.yaml"
    monkeypatch.setattr(git, "_REVERT_LOCK_TIMEOUT_SECONDS", 0.05)

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    _setup_old_repo(repo_dir)

    # A bare return value satisfies the rev-parse; the other commands'
    # outputs are unused.
    mock_run_git_command.return_value = "old_sha"

    _, revert = git.clone_or_update(
        url=url, ref=None, refresh=TimePeriodSeconds(days=1), domain=domain
    )
    assert revert is not None

    holder, release = _hold_lock_in_thread(_lock_path(url, None, domain))
    calls_before = len(mock_run_git_command.call_args_list)
    with caplog.at_level(logging.INFO):
        assert revert() is False
    release.set()
    holder.join()

    # No git reset was issued; the wait and the skip were both logged.
    assert len(mock_run_git_command.call_args_list) == calls_before
    assert any("Waiting for another process" in r.getMessage() for r in caplog.records)
    assert any("skipping revert" in r.getMessage() for r in caplog.records)


def test_update_clears_marker_while_rewriting(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """The completion marker is absent while a refresh rewrites the entry
    and restored once the rewrite finishes, so a timed-out peer's fallback
    never trusts a mid-rewrite tree and a crashed update re-clones."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    _setup_old_repo(repo_dir)

    marker_during_rewrite: list[bool] = []

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) in ("stash", "fetch", "reset"):
            marker_during_rewrite.append(_marker_path(repo_dir).is_file())
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    git.clone_or_update(
        url=url, ref=None, refresh=TimePeriodSeconds(days=1), domain=domain
    )

    assert marker_during_rewrite == [False, False, False]
    assert _marker_path(repo_dir).is_file()


def test_revert_skips_when_checkout_moved(
    tmp_path: Path, mock_run_git_command: Mock, caplog: pytest.LogCaptureFixture
) -> None:
    """revert() only undoes this process's own update; when another process
    refreshed the entry in the meantime it skips instead of rolling the
    peer's newer checkout backwards."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    _setup_old_repo(repo_dir)

    # Pre-update SHA, post-update SHA, then a peer's SHA at revert time.
    shas = iter(["old_sha", "new_sha", "peer_sha"])

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "rev-parse":
            return next(shas)
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    _, revert = git.clone_or_update(
        url=url, ref=None, refresh=TimePeriodSeconds(days=1), domain=domain
    )
    assert revert is not None

    with caplog.at_level(logging.WARNING):
        assert revert() is False

    resets = [
        c[0][0]
        for c in mock_run_git_command.call_args_list
        if _get_git_command_type(c[0][0]) == "reset" and c[0][0][-1] == "old_sha"
    ]
    assert resets == []
    assert any("checkout moved" in r.getMessage() for r in caplog.records)


def test_revert_returns_false_when_reset_fails(
    tmp_path: Path, mock_run_git_command: Mock, caplog: pytest.LogCaptureFixture
) -> None:
    """A failed git reset inside revert() is reported through the bool
    contract instead of raising a cv.Invalid that would replace the
    caller's original error."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    _setup_old_repo(repo_dir)

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        if _get_git_command_type(cmd) == "rev-parse":
            return "old_sha"
        # Only revert's reset targets the recorded SHA; the update path's
        # reset targets FETCH_HEAD and must succeed.
        if cmd[-1] == "old_sha":
            raise git.GitCommandError("object not found")
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    _, revert = git.clone_or_update(
        url=url, ref=None, refresh=TimePeriodSeconds(days=1), domain=domain
    )
    assert revert is not None

    with caplog.at_level(logging.WARNING):
        assert revert() is False

    assert any("Could not revert" in r.getMessage() for r in caplog.records)
    # The entry cannot be trusted after a failed reset; the dropped marker
    # forces a re-clone on the next use.
    assert not _marker_path(repo_dir).is_file()


def _raise_oserror_on_acquire(
    monkeypatch: pytest.MonkeyPatch, code: int = errno.ENOLCK
) -> None:
    """Make every FileLock acquire fail with the given errno."""

    def broken_acquire(self: FileLock, *args: Any, **kwargs: Any) -> None:
        raise OSError(code, os.strerror(code))

    monkeypatch.setattr(FileLock, "acquire", broken_acquire)


@pytest.mark.parametrize(
    ("code", "expected_fragment"),
    [
        # Genuinely missing lock support is reported as such.
        (errno.ENOLCK, "does not support locking"),
        # A cache directory problem is not blamed on lock support; git
        # reports the real error when it actually matters.
        (errno.EROFS, "Could not take the cache entry lock"),
    ],
)
def test_clone_or_update_continues_unlocked_when_filesystem_cannot_lock(
    tmp_path: Path,
    mock_run_git_command: Mock,
    caplog: pytest.LogCaptureFixture,
    monkeypatch: pytest.MonkeyPatch,
    code: int,
    expected_fragment: str,
) -> None:
    """A filesystem where taking the lock fails (e.g. NFS without a lock
    daemon) degrades to the old unlocked behavior instead of failing."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)
    _raise_oserror_on_acquire(monkeypatch, code)

    with caplog.at_level(logging.WARNING):
        result_dir, _ = git.clone_or_update(
            url=url, ref=None, refresh=git.NEVER_REFRESH, domain=domain
        )

    assert result_dir == repo_dir
    assert _marker_path(repo_dir).is_file()
    warnings = [
        r.getMessage()
        for r in caplog.records
        if "continuing without a lock" in r.getMessage()
    ]
    assert warnings
    assert expected_fragment in warnings[0]


@pytest.mark.parametrize("broken_from_start", [True, False])
def test_revert_continues_unlocked_when_filesystem_cannot_lock(
    tmp_path: Path,
    mock_run_git_command: Mock,
    caplog: pytest.LogCaptureFixture,
    monkeypatch: pytest.MonkeyPatch,
    broken_from_start: bool,
) -> None:
    """A revert still resets when locking is unavailable, whether the wrapper
    already fell back to unlocked (revert sees no lock at all) or the
    filesystem stops locking between the update and the revert."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    _setup_old_repo(repo_dir)

    # A bare return value satisfies the rev-parse; the other commands'
    # outputs are unused.
    mock_run_git_command.return_value = "old_sha"

    if broken_from_start:
        _raise_oserror_on_acquire(monkeypatch)

    with caplog.at_level(logging.WARNING):
        _, revert = git.clone_or_update(
            url=url, ref=None, refresh=TimePeriodSeconds(days=1), domain=domain
        )
        assert revert is not None
        if not broken_from_start:
            _raise_oserror_on_acquire(monkeypatch)
        # The reset ran (unlocked), so the revert reports success.
        assert revert() is True

    assert mock_run_git_command.call_args_list[-1][0][0] == [
        "git",
        "reset",
        "--hard",
        "old_sha",
    ]
    assert any("continuing without a lock" in r.getMessage() for r in caplog.records)


def _script_acquire_statuses(
    monkeypatch: pytest.MonkeyPatch, statuses: list["git._LockStatus"]
) -> list[float]:
    """Replace _acquire_repo_lock with a scripted sequence; returns the
    timeouts it was called with."""
    timeouts: list[float] = []
    status_iter = iter(statuses)

    def fake_acquire(
        lock: FileLock, safe_key: str, timeout: float, **kwargs: Any
    ) -> "git._LockStatus":
        timeouts.append(timeout)
        return next(status_iter)

    monkeypatch.setattr(git, "_acquire_repo_lock", fake_acquire)
    return timeouts


@pytest.mark.parametrize("subpath", [None, Path("lib")])
def test_clone_or_update_uses_complete_entry_when_lock_wait_times_out(
    tmp_path: Path,
    mock_run_git_command: Mock,
    caplog: pytest.LogCaptureFixture,
    monkeypatch: pytest.MonkeyPatch,
    subpath: Path | None,
) -> None:
    """A bounded wait behind a stalled holder falls back to an existing
    complete cache entry instead of hanging every peer."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    if subpath is not None:
        repo_dir = repo_dir / subpath
    _simulate_cloned_repo(repo_dir)
    _mark_clone_complete(repo_dir)

    timeouts = _script_acquire_statuses(monkeypatch, [git._LockStatus.TIMEOUT])

    with caplog.at_level(logging.WARNING):
        result_dir, revert = git.clone_or_update(
            url=url,
            ref=None,
            refresh=TimePeriodSeconds(days=1),
            domain=domain,
            subpath=subpath,
        )

    assert result_dir == repo_dir
    assert revert is None
    # Nothing was cloned or refreshed; the existing entry was used as-is.
    assert mock_run_git_command.call_args_list == []
    assert timeouts == [git._COMPLETE_ENTRY_LOCK_TIMEOUT_SECONDS]
    assert any(
        "proceeding with the existing clone" in r.getMessage() for r in caplog.records
    )


def test_clone_or_update_waits_unbounded_without_complete_entry(
    tmp_path: Path,
    mock_run_git_command: Mock,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """With no complete entry there is nothing to fall back to, so after the
    bounded wait expires the caller keeps waiting for the holder's clone."""
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    domain = "test"
    repo_dir = _compute_repo_dir(url, None, domain)
    mock_run_git_command.side_effect = _make_clone_side_effect(repo_dir)

    timeouts = _script_acquire_statuses(
        monkeypatch, [git._LockStatus.TIMEOUT, git._LockStatus.ACQUIRED]
    )

    result_dir, _ = git.clone_or_update(
        url=url, ref=None, refresh=git.NEVER_REFRESH, domain=domain
    )

    assert result_dir == repo_dir
    assert timeouts == [git._COMPLETE_ENTRY_LOCK_TIMEOUT_SECONDS, -1]
    # The clone proceeded normally once the lock was finally acquired.
    assert _marker_path(repo_dir).is_file()


def _real_git(*args: str, cwd: Path) -> None:
    """Run real git to build a test fixture repository."""
    subprocess.run(
        [
            "git",
            "-c",
            "user.email=test@test.invalid",
            "-c",
            "user.name=test",
            "-c",
            "commit.gpgsign=false",
            "-c",
            "protocol.file.allow=always",
            *args,
        ],
        cwd=cwd,
        check=True,
        capture_output=True,
    )


# Git blocks file-protocol submodules by default (CVE-2022-39253); the e2e
# tests allow them via GIT_CONFIG_* environment variables, which reach the
# child git processes through run_git_command's filtered environment.
_ALLOW_FILE_PROTOCOL_ENV = {
    "GIT_CONFIG_COUNT": "1",
    "GIT_CONFIG_KEY_0": "protocol.file.allow",
    "GIT_CONFIG_VALUE_0": "always",
}


def _make_real_repo(path: Path, filename: str) -> None:
    """Create a real git repository containing one committed file."""
    path.mkdir()
    _real_git("init", "-q", cwd=path)
    (path / filename).write_text("content")
    _real_git("add", filename, cwd=path)
    _real_git("commit", "-q", "-m", "init", cwd=path)


def _add_submodule(
    repo: Path, url: Path, path: str, *, update_none: bool = False
) -> None:
    """Add ``url`` as a submodule of ``repo`` at ``path`` and commit it."""
    _real_git("submodule", "add", str(url), path, cwd=repo)
    if update_none:
        _real_git(
            "config", "-f", ".gitmodules", f"submodule.{path}.update", "none", cwd=repo
        )
        _real_git("add", ".gitmodules", cwd=repo)
    _real_git("commit", "-q", "-m", f"add submodule {path}", cwd=repo)


def test_clone_or_update_real_git_without_submodules(tmp_path: Path) -> None:
    """End-to-end with real git: a repo with no .gitmodules clones cleanly.

    This is the issue #17860 scenario: requesting "all submodules" on a
    submodule-less repository must not invoke the git submodule porcelain
    and must produce a usable checkout.
    """
    CORE.config_path = tmp_path / "test.yaml"

    upstream = tmp_path / "upstream"
    _make_real_repo(upstream, "README.md")

    repo_dir, _ = git.clone_or_update(
        url=str(upstream),
        ref=None,
        refresh=None,
        domain="test_e2e",
        init_submodules=True,
    )

    assert (repo_dir / "README.md").is_file()


def test_clone_or_update_real_git_initializes_submodules(tmp_path: Path) -> None:
    """End-to-end with real git: submodules are actually checked out.

    Exercises the real `git submodule update` invocation, including the
    env handling in run_git_command that the mocked tests cannot cover.
    """
    CORE.config_path = tmp_path / "test.yaml"

    sub_repo = tmp_path / "sub"
    _make_real_repo(sub_repo, "sub_file.txt")

    upstream = tmp_path / "upstream"
    _make_real_repo(upstream, "README.md")
    _add_submodule(upstream, sub_repo, "vendor/sub")

    with patch.dict(os.environ, _ALLOW_FILE_PROTOCOL_ENV):
        repo_dir, _ = git.clone_or_update(
            url=str(upstream),
            ref=None,
            refresh=None,
            domain="test_e2e",
            init_submodules=True,
        )

    assert (repo_dir / "vendor" / "sub" / "sub_file.txt").is_file()


def test_clone_or_update_real_git_honors_update_none_submodule(
    tmp_path: Path,
) -> None:
    """End-to-end with real git: submodules declared `update = none` stay skipped.

    Shows git itself skipping the declared paths at both nesting levels
    (and exiting 0) while the regular submodules check out.
    """
    CORE.config_path = tmp_path / "test.yaml"

    sub_repo = tmp_path / "sub"
    _make_real_repo(sub_repo, "sub_file.txt")

    # Intermediate submodule that itself declares a skipped nested submodule.
    mid_repo = tmp_path / "mid"
    _make_real_repo(mid_repo, "mid_file.txt")
    _add_submodule(mid_repo, sub_repo, "vendor/leaf", update_none=True)

    upstream = tmp_path / "upstream"
    _make_real_repo(upstream, "README.md")
    _add_submodule(upstream, sub_repo, "vendor/sub")
    _add_submodule(upstream, sub_repo, "vendor/skipped", update_none=True)
    _add_submodule(upstream, mid_repo, "vendor/mid")

    with patch.dict(os.environ, _ALLOW_FILE_PROTOCOL_ENV):
        repo_dir, _ = git.clone_or_update(
            url=str(upstream),
            ref=None,
            refresh=None,
            domain="test_e2e",
            init_submodules=True,
        )

    assert (repo_dir / "vendor" / "sub" / "sub_file.txt").is_file()
    assert not (repo_dir / "vendor" / "skipped" / "sub_file.txt").exists()
    assert (repo_dir / "vendor" / "mid" / "mid_file.txt").is_file()
    assert not (
        repo_dir / "vendor" / "mid" / "vendor" / "leaf" / "sub_file.txt"
    ).exists()


def test_refresh_picks_up_new_remote_commits(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Shallow fetch must still pull new commits when the remote tip moves.

    Simulates a stale local repo at SHA "old" while the remote has advanced
    to SHA "new". The refresh path must run fetch (with --depth=1) followed
    by reset --hard FETCH_HEAD so the working tree advances to the new tip.
    """
    CORE.config_path = tmp_path / "test.yaml"

    url = "https://github.com/test/repo"
    ref = "main"
    domain = "test"
    repo_dir = _compute_repo_dir(url, ref, domain)

    _setup_old_repo(repo_dir)

    # rev-parse is called once before fetch to record the pre-update SHA.
    rev_parse_calls = {"count": 0}

    def git_command_side_effect(
        cmd: list[str], cwd: str | None = None, **kwargs: Any
    ) -> str:
        cmd_type = _get_git_command_type(cmd)
        if cmd_type == "rev-parse":
            rev_parse_calls["count"] += 1
            return "old_sha"
        return ""

    mock_run_git_command.side_effect = git_command_side_effect

    _, revert = git.clone_or_update(
        url=url, ref=ref, refresh=TimePeriodSeconds(days=1), domain=domain
    )

    # Verify the refresh sequence: rev-parse -> stash -> fetch (depth=1) -> reset
    call_list = mock_run_git_command.call_args_list
    cmd_sequence = [_get_git_command_type(c[0][0]) for c in call_list]
    # The trailing rev-parse records the post-update SHA for revert().
    assert cmd_sequence == ["rev-parse", "stash", "fetch", "reset", "rev-parse"]

    fetch_cmd = call_list[2][0][0]
    assert "--depth=1" in fetch_cmd
    assert fetch_cmd[-1] == ref

    reset_cmd = call_list[3][0][0]
    assert reset_cmd[-1] == "FETCH_HEAD"

    # revert callback should reset back to the recorded pre-update SHA.
    assert revert is not None
    assert revert() is True
    assert mock_run_git_command.call_args_list[-1][0][0] == [
        "git",
        "reset",
        "--hard",
        "old_sha",
    ]


def test_resolve_symlink_stub_returns_none_on_non_windows(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """On non-Windows, resolve_symlink_stub returns None without calling git."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    stub = repo_dir / "file.yaml"
    stub.write_text("static/file.yaml")

    with patch("esphome.git.sys.platform", "linux"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None
    mock_run_git_command.assert_not_called()


def test_resolve_symlink_stub_returns_target_for_mode_120000(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A mode-120000 file is recognised as a stub; its target Path is returned."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    (repo_dir / "static").mkdir()

    target = repo_dir / "static" / "real.yaml"
    target.write_text("esphome:\n  name: real\n")

    stub = repo_dir / "real.yaml"
    stub.write_text("static/real.yaml")

    mock_run_git_command.return_value = "120000 abc123 0\treal.yaml"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result == target.resolve()
    # Stub file itself was not modified — only inspected.
    assert stub.read_text() == "static/real.yaml"


def test_resolve_symlink_stub_resolves_relative_parent_paths(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Symlink targets with ``..`` segments resolve correctly within the repo."""
    repo_dir = tmp_path / "repo"
    (repo_dir / "subdir").mkdir(parents=True)
    (repo_dir / "static").mkdir()

    target = repo_dir / "static" / "shared.yaml"
    target.write_text("shared content")

    stub = repo_dir / "subdir" / "shared.yaml"
    stub.write_text("../static/shared.yaml")

    mock_run_git_command.return_value = "120000 abc123 0\tsubdir/shared.yaml"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result == target.resolve()


def test_resolve_symlink_stub_refuses_escape_outside_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A symlink pointing outside the repository is not followed."""
    outside = tmp_path / "outside.yaml"
    outside.write_text("sensitive")

    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "escape.yaml"
    stub.write_text("../outside.yaml")

    mock_run_git_command.return_value = "120000 abc123 0\tescape.yaml"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_returns_none_for_real_symlink(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A real symlink already opens transparently, so the helper short-circuits.

    Skipped on Windows where symlink creation requires
    SeCreateSymbolicLinkPrivilege.
    """
    if os.name == "nt":
        pytest.skip("Requires symlink-creation privilege on Windows")

    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    target = repo_dir / "real.yaml"
    target.write_text("real content")

    real_link = repo_dir / "link.yaml"
    real_link.symlink_to("real.yaml")

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, real_link)

    assert result is None
    # No git call needed for real symlinks.
    mock_run_git_command.assert_not_called()


def test_resolve_symlink_stub_returns_none_for_regular_file(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A regular file (mode 100644) whose content looks path-shaped is not
    followed."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    regular = repo_dir / "looks_like_path.txt"
    regular.write_text("static/something.yaml")

    mock_run_git_command.return_value = "100644 abc123 0\tlooks_like_path.txt"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, regular)

    assert result is None


def test_resolve_symlink_stub_returns_none_when_git_fails(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """If ``git ls-files`` fails (e.g. not a repo), the helper returns None."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "real.yaml"
    stub.write_text("static/real.yaml")

    mock_run_git_command.side_effect = GitCommandError("ls-files exploded")

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_returns_none_for_non_utf8_content(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A file whose bytes are not valid UTF-8 must not raise — return None."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "binary.bin"
    stub.write_bytes(b"\xff\xfe\x00\xff")

    mock_run_git_command.return_value = "120000 abc123 0\tbinary.bin"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_preserves_whitespace_in_target(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Only trailing CR/LF is stripped — internal whitespace is preserved."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    target_dir = repo_dir / "dir with spaces"
    target_dir.mkdir()
    target = target_dir / "real.yaml"
    target.write_text("hello")

    stub = repo_dir / "link.yaml"
    # Trailing newline (as git's checkout may append) is stripped, but
    # whitespace inside the target path itself must survive.
    stub.write_bytes(b"dir with spaces/real.yaml\n")

    mock_run_git_command.return_value = "120000 abc123 0\tlink.yaml"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result == target.resolve()


def test_resolve_symlink_stub_returns_none_for_directory_target(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A symlink pointing at a directory has no file content to load."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()
    (repo_dir / "dir_target").mkdir()

    stub = repo_dir / "link_to_dir"
    stub.write_text("dir_target")

    mock_run_git_command.return_value = "120000 abc123 0\tlink_to_dir"

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_returns_none_when_resolve_raises(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Path.resolve() raising (e.g. on a malformed target) must not propagate."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "broken.yaml"
    stub.write_text("ignored")

    mock_run_git_command.return_value = "120000 abc123 0\tbroken.yaml"

    with (
        patch("esphome.git.sys.platform", "win32"),
        patch.object(Path, "resolve", side_effect=OSError("bad path")),
    ):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_returns_none_when_file_missing(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A file path that doesn't exist is rejected before git is consulted."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    missing = repo_dir / "ghost.yaml"  # not created

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, missing)

    assert result is None
    mock_run_git_command.assert_not_called()


def test_resolve_symlink_stub_returns_none_when_path_outside_repo(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """A file path that isn't under repo_dir is rejected (ValueError from relative_to)."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    outside = tmp_path / "stray.yaml"
    outside.write_text("something")

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, outside)

    assert result is None
    mock_run_git_command.assert_not_called()


def test_resolve_symlink_stub_returns_none_when_untracked(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """Empty `git ls-files` output (untracked file) makes the helper return None."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "untracked.yaml"
    stub.write_text("static/foo.yaml")

    mock_run_git_command.return_value = ""

    with patch("esphome.git.sys.platform", "win32"):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None


def test_resolve_symlink_stub_returns_none_when_read_bytes_raises(
    tmp_path: Path, mock_run_git_command: Mock
) -> None:
    """An OSError from read_bytes() (e.g. file vanished mid-call) must not propagate."""
    repo_dir = tmp_path / "repo"
    repo_dir.mkdir()

    stub = repo_dir / "racy.yaml"
    stub.write_text("static/racy.yaml")

    mock_run_git_command.return_value = "120000 abc123 0\tracy.yaml"

    with (
        patch("esphome.git.sys.platform", "win32"),
        patch.object(Path, "read_bytes", side_effect=OSError("vanished")),
    ):
        result = git.resolve_symlink_stub(repo_dir, stub)

    assert result is None
