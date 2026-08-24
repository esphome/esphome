"""Unit tests for script/clang_tidy_hash.py module."""

from pathlib import Path
import sys

import pytest

# Add the script directory to Python path so we can import clang_tidy_hash
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "script"))

import clang_tidy_hash  # noqa: E402
from clang_tidy_hash import CLANG_TIDY_GLOBAL_FILES  # noqa: E402


def _populate(repo_root: Path) -> None:
    """Create every clang-tidy global file plus a base sdkconfig.defaults."""
    for name in CLANG_TIDY_GLOBAL_FILES:
        path = repo_root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f"contents of {name}\n")
    (repo_root / "sdkconfig.defaults").write_text("CONFIG_BASE=y\n")


def test_calculate_clang_tidy_hash_is_deterministic(tmp_path: Path) -> None:
    """Same inputs must produce the same hash."""
    _populate(tmp_path)
    assert clang_tidy_hash.calculate_clang_tidy_hash(
        repo_root=tmp_path
    ) == clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)


@pytest.mark.parametrize("filename", CLANG_TIDY_GLOBAL_FILES)
def test_calculate_clang_tidy_hash_changes_with_each_global_file(
    tmp_path: Path, filename: str
) -> None:
    """Editing any global file must change the hash."""
    _populate(tmp_path)
    before = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)

    (tmp_path / filename).write_text("changed\n")
    after = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)

    assert after != before


def test_calculate_clang_tidy_hash_includes_per_target_sdkconfig(
    tmp_path: Path,
) -> None:
    """Per-target sdkconfig.defaults.<target> files must be part of the hash."""
    _populate(tmp_path)
    before = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)

    # Adding a per-target file must change the hash.
    per_target = tmp_path / "sdkconfig.defaults.esp32c6"
    per_target.write_bytes(b"CONFIG_OPENTHREAD_ENABLED=y\n")
    after_add = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)
    assert after_add != before

    # Editing the per-target file must change the hash again.
    per_target.write_bytes(b"CONFIG_OPENTHREAD_ENABLED=n\n")
    after_edit = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)
    assert after_edit != after_add


def test_calculate_clang_tidy_hash_handles_missing_optional_files(
    tmp_path: Path,
) -> None:
    """Hash calculation must not fail when files are absent."""
    # Only .clang-tidy present; everything else missing.
    (tmp_path / ".clang-tidy").write_text("Checks: '-*'\n")
    result = clang_tidy_hash.calculate_clang_tidy_hash(repo_root=tmp_path)
    assert len(result) == 64  # sha256 hexdigest length


def test_read_file_bytes(tmp_path: Path) -> None:
    """Test read_file_bytes helper function."""
    test_file = tmp_path / "test.bin"
    test_content = b"binary content\x00\xff"
    test_file.write_bytes(test_content)

    result = clang_tidy_hash.read_file_bytes(test_file)

    assert result == test_content


def test_is_cached_missing_file_returns_false(tmp_path: Path) -> None:
    """A hash file that doesn't exist is never cached."""
    assert not clang_tidy_hash.is_cached(tmp_path / "missing.hash")


def test_update_cache_then_is_cached_true(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A freshly written cache file is reported as cached."""
    monkeypatch.setattr(clang_tidy_hash, "calculate_clang_tidy_hash", lambda: "abc123")
    hash_path = tmp_path / ".compile_commands.hash"

    clang_tidy_hash.update_cache(hash_path)

    assert clang_tidy_hash.is_cached(hash_path)


def test_is_cached_false_after_global_hash_changes(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A cache written under one hash is stale once the underlying hash changes."""
    monkeypatch.setattr(clang_tidy_hash, "calculate_clang_tidy_hash", lambda: "abc123")
    hash_path = tmp_path / ".compile_commands.hash"
    clang_tidy_hash.update_cache(hash_path)

    monkeypatch.setattr(clang_tidy_hash, "calculate_clang_tidy_hash", lambda: "def456")

    assert not clang_tidy_hash.is_cached(hash_path)


def test_is_cached_distinguishes_extra(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The nrf52 source-file list (passed as ``extra``) is part of the cache key."""
    monkeypatch.setattr(clang_tidy_hash, "calculate_clang_tidy_hash", lambda: "abc123")
    hash_path = tmp_path / ".compile_commands.hash"

    clang_tidy_hash.update_cache(hash_path, extra="file_a.cpp\nfile_b.cpp")

    assert clang_tidy_hash.is_cached(hash_path, extra="file_a.cpp\nfile_b.cpp")
    assert not clang_tidy_hash.is_cached(hash_path, extra="file_c.cpp")


def test_cache_key_empty_extra_equals_bare_hash(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """An empty ``extra`` must produce the same key as calculate_clang_tidy_hash() alone."""
    monkeypatch.setattr(clang_tidy_hash, "calculate_clang_tidy_hash", lambda: "abc123")

    assert clang_tidy_hash._cache_key("") == clang_tidy_hash.calculate_clang_tidy_hash()
